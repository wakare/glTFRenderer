#include "BakeRayTracingDispatch.h"

#include "../../../RendererCore/Private/InternalResourceHandleTable.h"

#include "RenderPass.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIInterface/IRHIMemoryManager.h"
#include "RHIDX12Impl/DX12Buffer.h"
#include "RHIDX12Impl/DX12CommandList.h"
#include "RHIDX12Impl/DX12ConverterUtils.h"
#include "RHIDX12Impl/DX12Texture.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace LightingBaker
{
    namespace
    {
        constexpr wchar_t kShaderRelativePathFromOutput[] = L"Resources/Shaders/BakeRayTracingDebug.hlsl";
        constexpr wchar_t kShaderRelativePathFromRepoRoot[] = L"glTFRenderer/LightingBaker/Resources/Shaders/BakeRayTracingDebug.hlsl";
        constexpr char kDispatchGroupName[] = "LightingBaker";
        constexpr char kDispatchPassName[] = "BakeRayTracingDispatch";
        constexpr std::size_t kOutputChannelCount = 4u;
        constexpr float kDefaultRayEpsilon = 0.01f;
        constexpr float kDefaultSkyIntensity = 1.0f;
        constexpr float kDefaultSkyGroundMix = 0.35f;
        constexpr std::size_t kEnvironmentImportanceBinCount = 32u;
        constexpr std::size_t kEnvironmentImportancePackedFloat4Count = kEnvironmentImportanceBinCount / 4u;
        constexpr float kEnvironmentImportanceFloor = 1e-4f;

        static_assert((kEnvironmentImportanceBinCount % 4u) == 0u);

        void FlushRecordedCommands(RendererInterface::ResourceOperator& resource_operator)
        {
            resource_operator.WaitFrameRenderFinished();
            resource_operator.WaitGPUIdle();
        }

        struct BakeRayTracingTexelRecordGPU
        {
            std::array<float, 4u> world_position_and_valid{0.0f, 0.0f, 0.0f, 0.0f};
            std::array<float, 4u> geometric_normal_and_padding{0.0f, 1.0f, 0.0f, 0.0f};
        };

        static_assert(sizeof(BakeRayTracingTexelRecordGPU) == sizeof(float) * 8u);

        struct BakeRayTracingDispatchConstantsGPU
        {
            std::uint32_t atlas_width{0u};
            std::uint32_t atlas_height{0u};
            std::uint32_t sample_index{0u};
            std::uint32_t sample_count{1u};
            std::uint32_t max_bounces{1u};
            std::uint32_t direct_light_sample_count{1u};
            std::uint32_t environment_light_sample_count{0u};
            std::uint32_t scene_light_count{0u};
            std::uint32_t emissive_triangle_count{0u};
            std::uint32_t padding_uint0{0u};
            std::uint32_t padding_uint1{0u};
            std::uint32_t padding_uint2{0u};
            float ray_epsilon{kDefaultRayEpsilon};
            float sky_intensity{kDefaultSkyIntensity};
            float sky_ground_mix{kDefaultSkyGroundMix};
            float padding0{0.0f};
            std::array<std::array<float, 4u>, kEnvironmentImportancePackedFloat4Count> environment_importance_cdf{};
        };

        static_assert((sizeof(BakeRayTracingDispatchConstantsGPU) % 16u) == 0u);

        BakeSceneValidationMessage MakeMessage(std::string code, std::string message)
        {
            BakeSceneValidationMessage result{};
            result.code = std::move(code);
            result.message = std::move(message);
            return result;
        }

        float ComputeSkyLuminanceForDirectionY(float direction_y, float sky_intensity, float sky_ground_mix)
        {
            const float up = std::clamp(direction_y * 0.5f + 0.5f, 0.0f, 1.0f);
            const float horizon = std::clamp(1.0f - std::abs(direction_y), 0.0f, 1.0f);

            const std::array<float, 3u> sky_top = {0.52f, 0.66f, 0.84f};
            const std::array<float, 3u> sky_bottom = {0.08f, 0.10f, 0.14f};
            const std::array<float, 3u> horizon_tint = {0.08f, 0.0736f, 0.0624f};
            const std::array<float, 3u> ground = {
                sky_ground_mix * 0.06f,
                sky_ground_mix * 0.05f,
                sky_ground_mix * 0.04f,
            };

            std::array<float, 3u> sky{};
            std::array<float, 3u> radiance{};
            for (std::size_t channel_index = 0; channel_index < 3u; ++channel_index)
            {
                sky[channel_index] =
                    sky_bottom[channel_index] * (1.0f - up) +
                    sky_top[channel_index] * up +
                    horizon_tint[channel_index] * horizon;
                radiance[channel_index] =
                    (ground[channel_index] * (1.0f - up) + sky[channel_index] * up) * sky_intensity;
            }

            return 0.2126f * radiance[0] + 0.7152f * radiance[1] + 0.0722f * radiance[2];
        }

        void BuildEnvironmentImportanceCdf(float sky_intensity,
                                           float sky_ground_mix,
                                           std::array<std::array<float, 4u>, kEnvironmentImportancePackedFloat4Count>& out_cdf)
        {
            std::array<float, kEnvironmentImportanceBinCount> cumulative_weights{};
            const float bin_width = 2.0f / static_cast<float>(kEnvironmentImportanceBinCount);
            float total_weight = 0.0f;
            for (std::size_t bin_index = 0; bin_index < kEnvironmentImportanceBinCount; ++bin_index)
            {
                const float direction_y = -1.0f + (static_cast<float>(bin_index) + 0.5f) * bin_width;
                const float bin_weight =
                    (std::max)(ComputeSkyLuminanceForDirectionY(direction_y, sky_intensity, sky_ground_mix),
                               kEnvironmentImportanceFloor);
                total_weight += bin_weight;
                cumulative_weights[bin_index] = total_weight;
            }

            if (total_weight <= 0.0f)
            {
                for (std::size_t bin_index = 0; bin_index < kEnvironmentImportanceBinCount; ++bin_index)
                {
                    cumulative_weights[bin_index] =
                        static_cast<float>(bin_index + 1u) / static_cast<float>(kEnvironmentImportanceBinCount);
                }
            }
            else
            {
                for (std::size_t bin_index = 0; bin_index < kEnvironmentImportanceBinCount; ++bin_index)
                {
                    cumulative_weights[bin_index] /= total_weight;
                }
            }

            for (std::size_t packed_index = 0; packed_index < kEnvironmentImportancePackedFloat4Count; ++packed_index)
            {
                for (std::size_t component_index = 0; component_index < 4u; ++component_index)
                {
                    const std::size_t bin_index = packed_index * 4u + component_index;
                    out_cdf[packed_index][component_index] = cumulative_weights[bin_index];
                }
            }
        }

        std::filesystem::path GetExecutableDirectory()
        {
            std::array<wchar_t, MAX_PATH> buffer{};
            const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0u || length >= buffer.size())
            {
                return {};
            }

            std::filesystem::path executable_path(buffer.data(), buffer.data() + length);
            return executable_path.parent_path();
        }

        std::filesystem::path ResolveDispatchShaderPath()
        {
            const std::filesystem::path executable_directory = GetExecutableDirectory();
            const auto find_candidate_in_ancestors =
                [](std::filesystem::path start_directory, const std::filesystem::path& relative_path)
            {
                std::error_code error_code{};
                start_directory = std::filesystem::absolute(start_directory, error_code);
                if (error_code)
                {
                    return std::filesystem::path{};
                }

                for (unsigned depth = 0u; depth < 8u; ++depth)
                {
                    const std::filesystem::path candidate = start_directory / relative_path;
                    if (std::filesystem::exists(candidate, error_code) && !error_code)
                    {
                        return candidate;
                    }

                    const std::filesystem::path parent = start_directory.parent_path();
                    if (parent.empty() || parent == start_directory)
                    {
                        break;
                    }

                    start_directory = parent;
                }

                return std::filesystem::path{};
            };

            const std::array<std::filesystem::path, 6u> candidates = {
                find_candidate_in_ancestors(std::filesystem::current_path(), std::filesystem::path(kShaderRelativePathFromRepoRoot)),
                find_candidate_in_ancestors(executable_directory, std::filesystem::path(kShaderRelativePathFromRepoRoot)),
                std::filesystem::current_path() / std::filesystem::path(kShaderRelativePathFromRepoRoot),
                executable_directory.parent_path() / std::filesystem::path(kShaderRelativePathFromRepoRoot),
                std::filesystem::current_path() / std::filesystem::path(kShaderRelativePathFromOutput),
                executable_directory / std::filesystem::path(kShaderRelativePathFromOutput),
            };

            for (const std::filesystem::path& candidate : candidates)
            {
                if (candidate.empty())
                {
                    continue;
                }

                std::error_code error_code{};
                if (std::filesystem::exists(candidate, error_code) && !error_code)
                {
                    return std::filesystem::absolute(candidate, error_code).lexically_normal();
                }
            }

            return {};
        }

        RendererInterface::RenderExecuteCommand MakeTraceRays(unsigned width, unsigned height, unsigned depth)
        {
            RendererInterface::RenderExecuteCommand command{};
            command.type = RendererInterface::ExecuteCommandType::RAY_TRACING_COMMAND;
            command.parameter.ray_tracing_dispatch_parameter.dispatch_width = width;
            command.parameter.ray_tracing_dispatch_parameter.dispatch_height = height;
            command.parameter.ray_tracing_dispatch_parameter.dispatch_depth = depth;
            return command;
        }

        bool BuildDenseAtlasTexelRecords(const LightmapAtlasBuildResult& atlas_result,
                                        unsigned atlas_width,
                                        unsigned atlas_height,
                                        std::vector<BakeRayTracingTexelRecordGPU>& out_records,
                                        BakeRayTracingDispatchRunResult& out_result,
                                        std::wstring& out_error)
        {
            const std::uint64_t pixel_count_u64 =
                static_cast<std::uint64_t>(atlas_width) * static_cast<std::uint64_t>(atlas_height);
            if (pixel_count_u64 == 0u)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_invalid_atlas_extent",
                    "Bake ray tracing dispatch requires a non-zero atlas extent."));
                out_error = L"Bake ray tracing dispatch requires a non-zero atlas extent.";
                return false;
            }

            if (pixel_count_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_dense_texel_count_overflow",
                    "Bake ray tracing dispatch dense atlas texel count exceeded addressable CPU memory range."));
                out_error = L"Bake ray tracing dispatch dense atlas texel count exceeded addressable CPU memory range.";
                return false;
            }

            if (pixel_count_u64 > static_cast<std::uint64_t>(std::numeric_limits<unsigned>::max()))
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_dense_texel_count_summary_overflow",
                    "Bake ray tracing dispatch dense atlas texel count exceeded summary storage range."));
                out_error = L"Bake ray tracing dispatch dense atlas texel count exceeded summary storage range.";
                return false;
            }

            const std::size_t pixel_count = static_cast<std::size_t>(pixel_count_u64);
            out_records.assign(pixel_count, BakeRayTracingTexelRecordGPU{});

            for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
            {
                if (texel_record.texel_x >= atlas_width || texel_record.texel_y >= atlas_height)
                {
                    out_result.errors.push_back(MakeMessage(
                        "rt_dispatch_texel_out_of_range",
                        "Atlas texel record referenced coordinates outside the dispatch atlas extent."));
                    out_error = L"Atlas texel record referenced coordinates outside the dispatch atlas extent.";
                    return false;
                }

                const std::size_t pixel_index =
                    static_cast<std::size_t>(texel_record.texel_y) * static_cast<std::size_t>(atlas_width) +
                    static_cast<std::size_t>(texel_record.texel_x);
                if (pixel_index >= out_records.size())
                {
                    out_result.errors.push_back(MakeMessage(
                        "rt_dispatch_dense_texel_index_invalid",
                        "Bake ray tracing dispatch dense atlas indexing exceeded the allocated record buffer."));
                    out_error = L"Bake ray tracing dispatch dense atlas indexing exceeded the allocated record buffer.";
                    return false;
                }

                BakeRayTracingTexelRecordGPU& gpu_record = out_records[pixel_index];
                if (gpu_record.world_position_and_valid[3] > 0.5f)
                {
                    out_result.errors.push_back(MakeMessage(
                        "rt_dispatch_duplicate_texel_record",
                        "Bake ray tracing dispatch detected duplicate atlas texel ownership while building the dense texel buffer."));
                    out_error = L"Bake ray tracing dispatch detected duplicate atlas texel ownership while building the dense texel buffer.";
                    return false;
                }

                gpu_record.world_position_and_valid = {
                    texel_record.world_position.x,
                    texel_record.world_position.y,
                    texel_record.world_position.z,
                    1.0f,
                };
                gpu_record.geometric_normal_and_padding = {
                    texel_record.geometric_normal.x,
                    texel_record.geometric_normal.y,
                    texel_record.geometric_normal.z,
                    0.0f,
                };
            }

            out_result.dense_texel_record_count = static_cast<unsigned>(pixel_count_u64);
            return true;
        }

        float HalfBitsToFloat(std::uint16_t bits)
        {
            const std::uint32_t sign = (static_cast<std::uint32_t>(bits & 0x8000u)) << 16u;
            const std::uint32_t exponent = (bits >> 10u) & 0x1fu;
            const std::uint32_t mantissa = bits & 0x03ffu;

            std::uint32_t float_bits = 0u;
            if (exponent == 0u)
            {
                if (mantissa == 0u)
                {
                    float_bits = sign;
                }
                else
                {
                    std::uint32_t normalized_mantissa = mantissa;
                    int exponent_unbiased = -14;
                    while ((normalized_mantissa & 0x0400u) == 0u)
                    {
                        normalized_mantissa <<= 1u;
                        --exponent_unbiased;
                    }

                    normalized_mantissa &= 0x03ffu;
                    const std::uint32_t exponent_bits = static_cast<std::uint32_t>(exponent_unbiased + 127) << 23u;
                    float_bits = sign | exponent_bits | (normalized_mantissa << 13u);
                }
            }
            else if (exponent == 0x1fu)
            {
                float_bits = sign | 0x7f800000u | (mantissa << 13u);
            }
            else
            {
                const int exponent_unbiased = static_cast<int>(exponent) - 15 + 127;
                float_bits = sign |
                             (static_cast<std::uint32_t>(exponent_unbiased) << 23u) |
                             (mantissa << 13u);
            }

            float value = 0.0f;
            std::memcpy(&value, &float_bits, sizeof(value));
            return value;
        }

        bool ReadBackDX12OutputTexture(RendererInterface::ResourceOperator& resource_operator,
                                       RendererInterface::RenderTargetHandle output_render_target,
                                       BakeRayTracingDispatchRunResult& out_result,
                                       std::wstring& out_error)
        {
            const auto render_target_allocation =
                RendererInterface::InternalResourceHandleTable::Instance().GetRenderTarget(output_render_target);
            if (!render_target_allocation || !render_target_allocation->m_source)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_missing_output_texture",
                    "Bake ray tracing dispatch could not resolve the output render target texture for readback."));
                out_error = L"Bake ray tracing dispatch could not resolve the output render target texture for readback.";
                return false;
            }

            auto* dx12_texture = dynamic_cast<DX12Texture*>(render_target_allocation->m_source.get());
            if (dx12_texture == nullptr)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_readback_unsupported_backend",
                    "Bake ray tracing dispatch readback currently supports DX12 render targets only."));
                out_error = L"Bake ray tracing dispatch readback currently supports DX12 render targets only.";
                return false;
            }

            const RHIMipMapCopyRequirements copy_requirements = dx12_texture->GetCopyReq();
            if (copy_requirements.row_pitch.empty() || copy_requirements.row_byte_size.empty() || copy_requirements.total_size == 0u)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_missing_copy_requirements",
                    "Bake ray tracing dispatch output texture is missing copy footprint metadata."));
                out_error = L"Bake ray tracing dispatch output texture is missing copy footprint metadata.";
                return false;
            }

            if (copy_requirements.row_pitch[0] > static_cast<std::size_t>(std::numeric_limits<unsigned>::max()))
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_row_pitch_overflow",
                    "Bake ray tracing dispatch output row pitch exceeded supported summary range."));
                out_error = L"Bake ray tracing dispatch output row pitch exceeded supported summary range.";
                return false;
            }

            const std::size_t pixel_count =
                static_cast<std::size_t>(out_result.output_width) * static_cast<std::size_t>(out_result.output_height);
            const std::size_t pixel_stride_bytes = sizeof(std::uint16_t) * kOutputChannelCount;
            const std::size_t expected_row_byte_size =
                static_cast<std::size_t>(out_result.output_width) * pixel_stride_bytes;
            if (copy_requirements.row_byte_size[0] < expected_row_byte_size)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_row_stride_invalid",
                    "Bake ray tracing dispatch output row footprint is smaller than RGBA16F atlas pixels require."));
                out_error = L"Bake ray tracing dispatch output row footprint is smaller than RGBA16F atlas pixels require.";
                return false;
            }

            IRHIMemoryManager& memory_manager = resource_operator.GetMemoryManager();
            std::shared_ptr<IRHIBufferAllocation> readback_buffer{};
            const RHIBufferDesc readback_desc{
                L"BakeRayTracingDispatchReadback",
                copy_requirements.total_size,
                1,
                1,
                RHIBufferType::Readback,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_COPY_DEST,
                static_cast<RHIResourceUsageFlags>(RUF_TRANSFER_DST | RUF_READBACK),
            };
            if (!memory_manager.AllocateBufferMemory(resource_operator.GetDevice(), readback_desc, readback_buffer) ||
                !readback_buffer || !readback_buffer->m_buffer)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_readback_buffer_failed",
                    "Failed to allocate a DX12 readback buffer for baker dispatch output."));
                out_error = L"Failed to allocate a DX12 readback buffer for baker dispatch output.";
                return false;
            }

            out_result.readback_buffer_allocated = true;

            IRHICommandList& command_list = resource_operator.GetCommandListForRecordPassCommand();
            auto* dx12_command_list = dynamic_cast<DX12CommandList*>(&command_list);
            auto* dx12_readback_buffer = dynamic_cast<DX12Buffer*>(readback_buffer->m_buffer.get());
            if (dx12_command_list == nullptr || dx12_readback_buffer == nullptr)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_readback_dx12_cast_failed",
                    "Bake ray tracing dispatch could not resolve DX12 command-list or buffer objects for readback."));
                out_error = L"Bake ray tracing dispatch could not resolve DX12 command-list or buffer objects for readback.";
                return false;
            }

            render_target_allocation->m_source->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
            readback_buffer->m_buffer->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);

            D3D12_TEXTURE_COPY_LOCATION src_location{};
            src_location.pResource = dx12_texture->GetRawResource();
            src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src_location.SubresourceIndex = 0u;

            D3D12_TEXTURE_COPY_LOCATION dst_location{};
            dst_location.pResource = dx12_readback_buffer->GetRawBuffer();
            dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst_location.PlacedFootprint.Offset = 0u;
            dst_location.PlacedFootprint.Footprint.Format =
                DX12ConverterUtils::ConvertToDXGIFormat(render_target_allocation->m_source->GetTextureFormat());
            dst_location.PlacedFootprint.Footprint.Width = out_result.output_width;
            dst_location.PlacedFootprint.Footprint.Height = out_result.output_height;
            dst_location.PlacedFootprint.Footprint.Depth = 1u;
            dst_location.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(copy_requirements.row_pitch[0]);

            const D3D12_BOX src_box{
                0u,
                0u,
                0u,
                static_cast<UINT>(out_result.output_width),
                static_cast<UINT>(out_result.output_height),
                1u,
            };
            dx12_command_list->GetCommandList()->CopyTextureRegion(&dst_location, 0u, 0u, 0u, &src_location, &src_box);
            out_result.readback_copy_recorded = true;

            FlushRecordedCommands(resource_operator);

            std::vector<std::uint8_t> readback_bytes(copy_requirements.total_size, 0u);
            if (!memory_manager.DownloadBufferData(*readback_buffer, readback_bytes.data(), readback_bytes.size()))
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_dispatch_readback_download_failed",
                    "Failed to download baker dispatch output bytes from the DX12 readback buffer."));
                out_error = L"Failed to download baker dispatch output bytes from the DX12 readback buffer.";
                return false;
            }

            out_result.readback_downloaded = true;
            out_result.output_row_pitch = static_cast<unsigned>(copy_requirements.row_pitch[0]);
            out_result.output_readback_size = copy_requirements.total_size;
            out_result.output_rgba.assign(pixel_count * kOutputChannelCount, 0.0f);
            out_result.output_nonzero_rgb_texel_count = 0u;
            out_result.output_nonzero_alpha_texel_count = 0u;
            out_result.output_trace_payload_sentinel_texel_count = 0u;
            out_result.output_rgb_sum = 0.0;
            out_result.output_luminance_sum = 0.0;

            for (unsigned y = 0u; y < out_result.output_height; ++y)
            {
                const std::uint8_t* row_begin =
                    readback_bytes.data() + static_cast<std::size_t>(y) * copy_requirements.row_pitch[0];
                const std::uint16_t* row_half_words = reinterpret_cast<const std::uint16_t*>(row_begin);
                for (unsigned x = 0u; x < out_result.output_width; ++x)
                {
                    const std::size_t src_offset = static_cast<std::size_t>(x) * kOutputChannelCount;
                    const std::size_t dst_offset =
                        (static_cast<std::size_t>(y) * static_cast<std::size_t>(out_result.output_width) +
                         static_cast<std::size_t>(x)) * kOutputChannelCount;
                    const float output_r = HalfBitsToFloat(row_half_words[src_offset + 0u]);
                    const float output_g = HalfBitsToFloat(row_half_words[src_offset + 1u]);
                    const float output_b = HalfBitsToFloat(row_half_words[src_offset + 2u]);
                    const float output_a = HalfBitsToFloat(row_half_words[src_offset + 3u]);
                    out_result.output_rgba[dst_offset + 0u] = output_r;
                    out_result.output_rgba[dst_offset + 1u] = output_g;
                    out_result.output_rgba[dst_offset + 2u] = output_b;
                    out_result.output_rgba[dst_offset + 3u] = output_a;

                    const double positive_r = static_cast<double>((std::max)(output_r, 0.0f));
                    const double positive_g = static_cast<double>((std::max)(output_g, 0.0f));
                    const double positive_b = static_cast<double>((std::max)(output_b, 0.0f));
                    out_result.output_rgb_sum += positive_r + positive_g + positive_b;
                    out_result.output_luminance_sum +=
                        positive_r * 0.2126 +
                        positive_g * 0.7152 +
                        positive_b * 0.0722;

                    const bool nonzero_rgb =
                        std::abs(output_r) > 1e-6f ||
                        std::abs(output_g) > 1e-6f ||
                        std::abs(output_b) > 1e-6f;
                    if (nonzero_rgb)
                    {
                        ++out_result.output_nonzero_rgb_texel_count;
                    }

                    if (std::abs(output_a) > 1e-6f)
                    {
                        ++out_result.output_nonzero_alpha_texel_count;
                    }

                    const bool trace_payload_sentinel =
                        output_r <= -0.5f &&
                        output_g <= -1.5f &&
                        output_b <= -2.5f;
                    if (trace_payload_sentinel)
                    {
                        ++out_result.output_trace_payload_sentinel_texel_count;
                    }
                }
            }

            out_result.output_payload_valid = true;
            return true;
        }
    }

    bool BakeRayTracingDispatchRunResult::HasErrors() const
    {
        return !errors.empty();
    }

    bool BakeRayTracingDispatchRunResult::HasValidationErrors() const
    {
        return HasErrors();
    }

    bool BakeRayTracingDispatchRunner::RunDispatch(const LightmapAtlasBuildResult& atlas_result,
                                                   BakeRayTracingRuntimeBuildResult& runtime_result,
                                                   const BakeRayTracingDispatchSettings& settings,
                                                   BakeRayTracingDispatchRunResult& out_result,
                                                   std::wstring& out_error) const
    {
        out_result = BakeRayTracingDispatchRunResult{};
        out_result.atlas_resolution = atlas_result.atlas_resolution;
        out_result.texel_record_count = atlas_result.texel_record_count;
        out_error.clear();

        if (!runtime_result.window || !runtime_result.resource_operator || !runtime_result.acceleration_structure_initialized ||
            !runtime_result.acceleration_structure)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_runtime_unavailable",
                "Ray tracing dispatch requires an initialized baker runtime with a valid TLAS."));
            out_error = L"Ray tracing dispatch requires an initialized baker runtime with a valid TLAS.";
            return false;
        }

        if (!runtime_result.scene_vertex_buffer_handle.IsValid() ||
            !runtime_result.scene_index_buffer_handle.IsValid() ||
            !runtime_result.scene_instance_buffer_handle.IsValid() ||
            !runtime_result.emissive_triangle_buffer_handle.IsValid())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_runtime_shading_buffers_missing",
                "Ray tracing dispatch requires shading vertex, index, instance, and emissive-triangle buffers from the baker runtime."));
            out_error =
                L"Ray tracing dispatch requires shading vertex, index, instance, and emissive-triangle buffers from the baker runtime.";
            return false;
        }

        if (atlas_result.HasValidationErrors())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_invalid_atlas",
                "Ray tracing dispatch requires a validation-passed atlas build result."));
            out_error = L"Ray tracing dispatch requires a validation-passed atlas build result.";
            return false;
        }

        if (atlas_result.texel_record_count == 0u)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_empty_atlas",
                "Ray tracing dispatch requires atlas texel coverage to execute."));
            out_error = L"Ray tracing dispatch requires atlas texel coverage to execute.";
            return false;
        }

        const std::filesystem::path shader_path = ResolveDispatchShaderPath();
        if (shader_path.empty())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_missing_shader",
                "Failed to resolve BakeRayTracingDebug.hlsl from the current workspace or output directory."));
            out_error = L"Failed to resolve BakeRayTracingDebug.hlsl from the current workspace or output directory.";
            return false;
        }

        out_result.shader_path = shader_path;
        out_result.shader_path_resolved = true;
        out_result.dispatch_width = settings.dispatch_width > 0u ? settings.dispatch_width : atlas_result.atlas_resolution;
        out_result.dispatch_height = settings.dispatch_height > 0u ? settings.dispatch_height : atlas_result.atlas_resolution;
        out_result.dispatch_depth = settings.dispatch_depth;
        out_result.sample_index = settings.sample_index;
        out_result.sample_count = settings.sample_count;
        out_result.max_bounces = (std::max)(1u, settings.max_bounces);
        out_result.direct_light_sample_count = settings.direct_light_sample_count;
        out_result.environment_light_sample_count = settings.environment_light_sample_count;
        out_result.sky_intensity = (std::max)(0.0f, settings.sky_intensity);
        out_result.sky_ground_mix = std::clamp(settings.sky_ground_mix, 0.0f, 1.0f);
        out_result.scene_light_count = static_cast<unsigned>(runtime_result.uploaded_scene_light_count);
        out_result.emissive_triangle_count = static_cast<unsigned>(runtime_result.uploaded_emissive_triangle_count);
        out_result.output_width = out_result.dispatch_width;
        out_result.output_height = out_result.dispatch_height;

        if (out_result.dispatch_width != atlas_result.atlas_resolution ||
            out_result.dispatch_height != atlas_result.atlas_resolution)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_dimension_mismatch",
                "Bake ray tracing dispatch currently requires atlas-sized dispatch dimensions."));
            out_error = L"Bake ray tracing dispatch currently requires atlas-sized dispatch dimensions.";
            return false;
        }

        if (out_result.dispatch_depth != 1u)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_depth_unsupported",
                "Bake ray tracing dispatch currently supports a dispatch depth of exactly 1."));
            out_error = L"Bake ray tracing dispatch currently supports a dispatch depth of exactly 1.";
            return false;
        }

        std::vector<BakeRayTracingTexelRecordGPU> dense_texel_records{};
        if (!BuildDenseAtlasTexelRecords(
                atlas_result,
                out_result.dispatch_width,
                out_result.dispatch_height,
                dense_texel_records,
                out_result,
                out_error))
        {
            return false;
        }

        RendererInterface::ResourceOperator& resource_operator = *runtime_result.resource_operator;
        RendererInterface::BufferDesc texel_record_buffer_desc{};
        texel_record_buffer_desc.name = "BakeRayTracingTexelRecords";
        texel_record_buffer_desc.usage = RendererInterface::USAGE_SRV;
        texel_record_buffer_desc.type = RendererInterface::DEFAULT;
        texel_record_buffer_desc.size =
            dense_texel_records.size() * sizeof(BakeRayTracingTexelRecordGPU);
        texel_record_buffer_desc.data = dense_texel_records.data();
        const RendererInterface::BufferHandle texel_record_buffer_handle =
            resource_operator.CreateBuffer(texel_record_buffer_desc);
        if (!texel_record_buffer_handle.IsValid())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_texel_record_buffer_failed",
                "Failed to allocate the dense atlas texel record SRV buffer for baker dispatch."));
            out_error = L"Failed to allocate the dense atlas texel record SRV buffer for baker dispatch.";
            return false;
        }

        out_result.texel_record_buffer_created = true;

        BakeRayTracingDispatchConstantsGPU dispatch_constants{};
        dispatch_constants.atlas_width = out_result.dispatch_width;
        dispatch_constants.atlas_height = out_result.dispatch_height;
        dispatch_constants.sample_index = out_result.sample_index;
        dispatch_constants.sample_count = out_result.sample_count;
        dispatch_constants.max_bounces = out_result.max_bounces;
        dispatch_constants.direct_light_sample_count = out_result.direct_light_sample_count;
        dispatch_constants.environment_light_sample_count = out_result.environment_light_sample_count;
        dispatch_constants.scene_light_count = out_result.scene_light_count;
        dispatch_constants.emissive_triangle_count = out_result.emissive_triangle_count;
        dispatch_constants.ray_epsilon = kDefaultRayEpsilon;
        dispatch_constants.sky_intensity = out_result.sky_intensity;
        dispatch_constants.sky_ground_mix = out_result.sky_ground_mix;
        BuildEnvironmentImportanceCdf(
            out_result.sky_intensity,
            out_result.sky_ground_mix,
            dispatch_constants.environment_importance_cdf);

        RendererInterface::BufferDesc dispatch_constants_desc{};
        dispatch_constants_desc.name = "BakeRayTracingDispatchConstants";
        dispatch_constants_desc.usage = RendererInterface::USAGE_CBV;
        dispatch_constants_desc.type = RendererInterface::DEFAULT;
        dispatch_constants_desc.size = sizeof(dispatch_constants);
        dispatch_constants_desc.data = &dispatch_constants;
        const RendererInterface::BufferHandle dispatch_constants_buffer_handle =
            resource_operator.CreateBuffer(dispatch_constants_desc);
        if (!dispatch_constants_buffer_handle.IsValid())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_constants_buffer_failed",
                "Failed to allocate the baker dispatch constants buffer."));
            out_error = L"Failed to allocate the baker dispatch constants buffer.";
            return false;
        }

        out_result.dispatch_constants_buffer_created = true;

        const auto output_usage = static_cast<RendererInterface::ResourceUsage>(
            RendererInterface::COPY_SRC |
            RendererInterface::COPY_DST |
            RendererInterface::SHADER_RESOURCE |
            RendererInterface::UNORDER_ACCESS);
        const RendererInterface::RenderTargetHandle output_render_target = resource_operator.CreateRenderTarget(
            "BakeRayTracingDispatchOutput",
            out_result.output_width,
            out_result.output_height,
            RendererInterface::RGBA16_FLOAT,
            RendererInterface::default_clear_color,
            output_usage);
        if (!output_render_target.IsValid())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_output_allocation_failed",
                "Failed to allocate the baker ray tracing output render target."));
            out_error = L"Failed to allocate the baker ray tracing output render target.";
            return false;
        }

        out_result.output_render_target_created = true;
        std::wcout << L"[LightingBaker][Dispatch] Output render target ready.\n";

        RendererInterface::ShaderDesc shader_desc{};
        shader_desc.shader_type = RendererInterface::RAY_TRACING_SHADER;
        shader_desc.shader_file_name = shader_path.string();
        shader_desc.entry_point.clear();
        const RendererInterface::ShaderHandle shader_handle = resource_operator.CreateShader(shader_desc);
        if (!shader_handle.IsValid())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_shader_compile_failed",
                "Failed to compile the baker ray tracing debug shader library."));
            out_error = L"Failed to compile the baker ray tracing debug shader library.";
            return false;
        }
        std::wcout << L"[LightingBaker][Dispatch] Ray tracing shader ready.\n";

        RendererInterface::RenderPassDesc render_pass_desc{};
        render_pass_desc.type = RendererInterface::RenderPassType::RAY_TRACING;
        render_pass_desc.shaders.emplace(RendererInterface::RAY_TRACING_SHADER, shader_handle);
        render_pass_desc.ray_tracing_desc = RendererInterface::RayTracingPassDesc{};
        render_pass_desc.ray_tracing_desc->config.payload_size = sizeof(float) * 16u;
        render_pass_desc.ray_tracing_desc->config.attribute_size = sizeof(float) * 2u;
        render_pass_desc.ray_tracing_desc->config.max_recursion_count = 1u;
        render_pass_desc.ray_tracing_desc->export_function_names = {
            out_result.raygen_entry,
            out_result.closest_hit_entry,
            out_result.any_hit_entry,
            out_result.miss_entry,
        };
        render_pass_desc.ray_tracing_desc->hit_group_descs.push_back({
            out_result.hit_group_export,
            out_result.closest_hit_entry,
            out_result.any_hit_entry,
            "",
        });
        render_pass_desc.ray_tracing_desc->acceleration_structure_bindings.push_back({
            out_result.acceleration_structure_binding_name,
            runtime_result.acceleration_structure,
        });
        render_pass_desc.ray_tracing_desc->shader_table = RHIResourceFactory::CreateRHIResource<IRHIShaderTable>();
        if (!render_pass_desc.ray_tracing_desc->shader_table)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_shader_table_allocation_failed",
                "Failed to allocate an IRHIShaderTable for the baker dispatch pass."));
            out_error = L"Failed to allocate an IRHIShaderTable for the baker dispatch pass.";
            return false;
        }

        const RendererInterface::RenderPassHandle render_pass_handle = resource_operator.CreateRenderPass(render_pass_desc);
        if (!render_pass_handle.IsValid())
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_render_pass_failed",
                "Failed to create the baker ray tracing render pass."));
            out_error = L"Failed to create the baker ray tracing render pass.";
            return false;
        }

        out_result.render_pass_created = true;
        std::wcout << L"[LightingBaker][Dispatch] Ray tracing render pass ready.\n";

        const std::shared_ptr<RenderPass> render_pass =
            RendererInterface::InternalResourceHandleTable::Instance().GetRenderPass(render_pass_handle);
        if (!render_pass)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_missing_render_pass",
                "Bake ray tracing dispatch could not resolve the created render pass resource."));
            out_error = L"Bake ray tracing dispatch could not resolve the created render pass resource.";
            return false;
        }

        out_result.acceleration_structure_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.acceleration_structure_binding_name);
        out_result.texel_record_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.texel_record_binding_name);
        out_result.scene_vertex_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.scene_vertex_binding_name);
        out_result.scene_index_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.scene_index_binding_name);
        out_result.scene_instance_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.scene_instance_binding_name);
        out_result.scene_light_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.scene_light_binding_name);
        out_result.emissive_triangle_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.emissive_triangle_binding_name);
        out_result.material_texture_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.material_texture_binding_name);
        out_result.dispatch_constants_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.dispatch_constants_binding_name);
        out_result.output_root_allocation_found =
            render_pass->HasRootSignatureAllocation(out_result.output_binding_name);

        std::vector<RHIShaderBindingTable> shader_binding_tables{};
        auto& shader_binding_table = shader_binding_tables.emplace_back();
        shader_binding_table.raygen_entry = out_result.raygen_entry;
        shader_binding_table.miss_entry = out_result.miss_entry;
        shader_binding_table.hit_group_entry = out_result.hit_group_export;

        IRHICommandList& command_list = resource_operator.GetCommandListForRecordPassCommand();
        if (!render_pass_desc.ray_tracing_desc->shader_table->InitShaderTable(
                resource_operator.GetDevice(),
                command_list,
                resource_operator.GetMemoryManager(),
                render_pass->GetPipelineStateObject(),
                *runtime_result.acceleration_structure,
                shader_binding_tables))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_shader_table_init_failed",
                "Failed to initialize the baker ray tracing shader table."));
            out_error = L"Failed to initialize the baker ray tracing shader table.";
            return false;
        }

        out_result.shader_table_initialized = true;
        std::wcout << L"[LightingBaker][Dispatch] Shader table ready.\n";

        // Dispatch resources are uploaded before the first render-graph frame advances to a new frame slot.
        // Flush them now so the upcoming ray-tracing pass does not read stale/default GPU contents.
        FlushRecordedCommands(resource_operator);
        out_result.pre_dispatch_uploads_flushed = true;

        RendererInterface::RenderPassDrawDesc draw_desc{};
        draw_desc.execute_commands.push_back(MakeTraceRays(
            out_result.dispatch_width,
            out_result.dispatch_height,
            out_result.dispatch_depth));
        RendererInterface::BufferBindingDesc texel_record_binding{};
        texel_record_binding.buffer_handle = texel_record_buffer_handle;
        texel_record_binding.binding_type = RendererInterface::BufferBindingDesc::SRV;
        texel_record_binding.stride = sizeof(BakeRayTracingTexelRecordGPU);
        texel_record_binding.count = out_result.dense_texel_record_count;
        texel_record_binding.is_structured_buffer = true;
        draw_desc.buffer_resources.emplace(out_result.texel_record_binding_name, texel_record_binding);

        RendererInterface::BufferBindingDesc scene_vertex_binding{};
        scene_vertex_binding.buffer_handle = runtime_result.scene_vertex_buffer_handle;
        scene_vertex_binding.binding_type = RendererInterface::BufferBindingDesc::SRV;
        scene_vertex_binding.stride = sizeof(BakeRayTracingSceneVertexGPU);
        scene_vertex_binding.count = static_cast<unsigned>(runtime_result.uploaded_shading_vertex_count);
        scene_vertex_binding.is_structured_buffer = true;
        draw_desc.buffer_resources.emplace(out_result.scene_vertex_binding_name, scene_vertex_binding);
        out_result.scene_vertex_buffer_bound = true;

        out_result.scene_index_buffer_bound = true;
        RendererInterface::BufferBindingDesc scene_index_binding{};
        scene_index_binding.buffer_handle = runtime_result.scene_index_buffer_handle;
        scene_index_binding.binding_type = RendererInterface::BufferBindingDesc::SRV;
        scene_index_binding.stride = sizeof(std::uint32_t);
        scene_index_binding.count = static_cast<unsigned>(runtime_result.uploaded_shading_index_count);
        scene_index_binding.is_structured_buffer = true;
        draw_desc.buffer_resources.emplace(out_result.scene_index_binding_name, scene_index_binding);

        out_result.scene_instance_buffer_bound = true;
        RendererInterface::BufferBindingDesc scene_instance_binding{};
        scene_instance_binding.buffer_handle = runtime_result.scene_instance_buffer_handle;
        scene_instance_binding.binding_type = RendererInterface::BufferBindingDesc::SRV;
        scene_instance_binding.stride = sizeof(BakeRayTracingSceneInstanceGPU);
        scene_instance_binding.count = static_cast<unsigned>(runtime_result.uploaded_shading_instance_count);
        scene_instance_binding.is_structured_buffer = true;
        draw_desc.buffer_resources.emplace(out_result.scene_instance_binding_name, scene_instance_binding);

        out_result.scene_light_buffer_bound = true;
        RendererInterface::BufferBindingDesc scene_light_binding{};
        scene_light_binding.buffer_handle = runtime_result.scene_light_buffer_handle;
        scene_light_binding.binding_type = RendererInterface::BufferBindingDesc::SRV;
        scene_light_binding.stride = sizeof(BakeRayTracingSceneLightGPU);
        scene_light_binding.count = (std::max)(1u, out_result.scene_light_count);
        scene_light_binding.is_structured_buffer = true;
        draw_desc.buffer_resources.emplace(out_result.scene_light_binding_name, scene_light_binding);

        out_result.emissive_triangle_buffer_bound = true;
        RendererInterface::BufferBindingDesc emissive_triangle_binding{};
        emissive_triangle_binding.buffer_handle = runtime_result.emissive_triangle_buffer_handle;
        emissive_triangle_binding.binding_type = RendererInterface::BufferBindingDesc::SRV;
        emissive_triangle_binding.stride = sizeof(BakeRayTracingSceneEmissiveTriangleGPU);
        emissive_triangle_binding.count = (std::max)(1u, out_result.emissive_triangle_count);
        emissive_triangle_binding.is_structured_buffer = true;
        draw_desc.buffer_resources.emplace(out_result.emissive_triangle_binding_name, emissive_triangle_binding);

        out_result.material_texture_table_bound = true;
        RendererInterface::TextureBindingDesc material_texture_binding{};
        material_texture_binding.type = RendererInterface::TextureBindingDesc::SRV;
        material_texture_binding.textures = runtime_result.material_texture_handles;
        draw_desc.texture_resources.emplace(out_result.material_texture_binding_name, material_texture_binding);

        RendererInterface::BufferBindingDesc dispatch_constants_binding{};
        dispatch_constants_binding.buffer_handle = dispatch_constants_buffer_handle;
        dispatch_constants_binding.binding_type = RendererInterface::BufferBindingDesc::CBV;
        draw_desc.buffer_resources.emplace(out_result.dispatch_constants_binding_name, dispatch_constants_binding);

        RendererInterface::RenderTargetTextureBindingDesc output_binding{};
        output_binding.name = out_result.output_binding_name;
        output_binding.render_target_texture.push_back(output_render_target);
        output_binding.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        draw_desc.render_target_texture_resources.emplace(output_binding.name, output_binding);

        RendererInterface::RenderGraphNodeDesc node_desc{};
        node_desc.render_pass_handle = render_pass_handle;
        node_desc.draw_info = std::move(draw_desc);
        node_desc.debug_group = kDispatchGroupName;
        node_desc.debug_name = kDispatchPassName;

        RendererInterface::RenderGraph render_graph(resource_operator, *runtime_result.window, false, false);
        const RendererInterface::RenderGraphNodeHandle render_graph_node_handle = render_graph.CreateRenderGraphNode(node_desc);
        if (!render_graph_node_handle.IsValid() || !render_graph.RegisterRenderGraphNode(render_graph_node_handle))
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_graph_registration_failed",
                "Failed to register the baker ray tracing node into the render graph."));
            out_error = L"Failed to register the baker ray tracing node into the render graph.";
            return false;
        }

        out_result.graph_node_registered = true;
        LOG_FORMAT_FLUSH("[LightingBaker][Dispatch] Render graph node registered.\n");
        render_graph.RegisterRenderTargetToColorOutput(output_render_target);
        render_graph.RegisterTickCallback([window = runtime_result.window, &out_result](unsigned long long)
        {
            if (!out_result.close_requested && window)
            {
                out_result.close_requested = true;
                window->RequestClose();
            }
        });

        out_result.compile_requested = render_graph.CompileRenderPassAndExecute();
        if (!out_result.compile_requested)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_compile_failed",
                "Bake ray tracing dispatch failed to bind the render graph to the window tick."));
            out_error = L"Bake ray tracing dispatch failed to bind the render graph to the window tick.";
            return false;
        }

        LOG_FORMAT_FLUSH("[LightingBaker][Dispatch] Render graph armed.\n");
        out_result.event_loop_entered = true;
        LOG_FORMAT_FLUSH("[LightingBaker][Dispatch] Entering window event loop.\n");
        runtime_result.window->EnterWindowEventLoop();
        resource_operator.WaitGPUIdle();
        LOG_FORMAT_FLUSH("[LightingBaker][Dispatch] Event loop exited.\n");

        out_result.frame_stats = render_graph.GetLastFrameStats();
        out_result.window_loop_timing = runtime_result.window->GetLastLoopTiming();
        out_result.frame_executed = out_result.frame_stats.executed_ray_tracing_pass_count > 0u;
        if (!out_result.frame_executed)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_dispatch_no_frame_execution",
                "Bake ray tracing dispatch did not execute a ray tracing pass before the window loop exited."));
            out_error = L"Bake ray tracing dispatch did not execute a ray tracing pass before the window loop exited.";
            return false;
        }

        if (!ReadBackDX12OutputTexture(resource_operator, output_render_target, out_result, out_error))
        {
            return false;
        }

        return true;
    }
}
