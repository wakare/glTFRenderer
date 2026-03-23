#include "LightmapAtlasBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace LightingBaker
{
    namespace
    {
        void AddMessage(std::vector<BakeSceneValidationMessage>& out_messages,
                        std::string code,
                        std::string message)
        {
            out_messages.push_back(BakeSceneValidationMessage{
                .code = std::move(code),
                .message = std::move(message),
            });
        }

        glm::fvec3 ComputeBarycentric(const glm::fvec2& p,
                                      const glm::fvec2& a,
                                      const glm::fvec2& b,
                                      const glm::fvec2& c,
                                      bool& out_valid)
        {
            const glm::fvec2 v0 = b - a;
            const glm::fvec2 v1 = c - a;
            const glm::fvec2 v2 = p - a;
            const float denominator = v0.x * v1.y - v1.x * v0.y;
            if (std::abs(denominator) <= 1e-12f)
            {
                out_valid = false;
                return glm::fvec3(0.0f, 0.0f, 0.0f);
            }

            const float inv_denominator = 1.0f / denominator;
            const float barycentric_b = (v2.x * v1.y - v1.x * v2.y) * inv_denominator;
            const float barycentric_c = (v0.x * v2.y - v2.x * v0.y) * inv_denominator;
            const float barycentric_a = 1.0f - barycentric_b - barycentric_c;
            out_valid = true;
            return glm::fvec3(barycentric_a, barycentric_b, barycentric_c);
        }

        bool IsInsideTriangle(const glm::fvec3& barycentric)
        {
            constexpr float epsilon = 1e-5f;
            return barycentric.x >= -epsilon &&
                   barycentric.y >= -epsilon &&
                   barycentric.z >= -epsilon;
        }

        glm::uvec2 ClampTexelCoord(int x, int y, unsigned resolution)
        {
            const int clamped_x = (std::max)(0, (std::min)(x, static_cast<int>(resolution) - 1));
            const int clamped_y = (std::max)(0, (std::min)(y, static_cast<int>(resolution) - 1));
            return glm::uvec2(static_cast<unsigned>(clamped_x), static_cast<unsigned>(clamped_y));
        }

        std::size_t ToTexelIndex(unsigned x, unsigned y, unsigned resolution)
        {
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(resolution) + static_cast<std::size_t>(x);
        }
    }

    bool LightmapAtlasBuildResult::HasErrors() const
    {
        return !errors.empty();
    }

    bool LightmapAtlasBuildResult::HasValidationErrors() const
    {
        return HasErrors();
    }

    bool LightmapAtlasBuilder::BuildAtlas(const BakeSceneImportResult& import_result,
                                          const LightmapAtlasBuildSettings& settings,
                                          LightmapAtlasBuildResult& out_result,
                                          std::wstring& out_error) const
    {
        out_result = LightmapAtlasBuildResult{};
        out_result.atlas_resolution = settings.atlas_resolution;
        out_result.texel_border = settings.texel_border;

        if (settings.atlas_resolution == 0u)
        {
            out_error = L"Atlas resolution must be non-zero.";
            return false;
        }

        if (import_result.HasValidationErrors())
        {
            AddMessage(out_result.errors, "import_validation", "Scene import result contains validation errors.");
            return true;
        }

        std::vector<int> texel_owner(settings.atlas_resolution * settings.atlas_resolution, -1);
        std::vector<std::uint8_t> texel_overlap_mask(settings.atlas_resolution * settings.atlas_resolution, 0u);

        for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
        {
            if (!primitive.can_emit_lightmap_binding)
            {
                continue;
            }

            if (!primitive.geometry.HasGeometry())
            {
                AddMessage(out_result.errors,
                           "missing_geometry",
                           "Primitive passed import validation but does not carry atlas geometry.");
                continue;
            }

            LightmapAtlasBindingInfo binding_info{};
            binding_info.atlas_id = 0u;
            binding_info.stable_node_key = primitive.stable_node_key;
            binding_info.primitive_hash = primitive.primitive_hash;
            binding_info.node_name = primitive.node_name;
            binding_info.mesh_name = primitive.mesh_name;

            const unsigned binding_index = static_cast<unsigned>(out_result.bindings.size());
            out_result.bindings.push_back(std::move(binding_info));
            LightmapAtlasBindingInfo& binding = out_result.bindings.back();

            const std::vector<unsigned>& indices = primitive.geometry.triangle_indices;
            const std::vector<glm::fvec2>& uv1_vertices = primitive.geometry.uv1_vertices;
            const std::vector<glm::fvec3>& world_positions = primitive.geometry.world_positions;
            for (unsigned triangle_begin = 0; triangle_begin + 2u < indices.size(); triangle_begin += 3u)
            {
                const unsigned index0 = indices[triangle_begin];
                const unsigned index1 = indices[triangle_begin + 1u];
                const unsigned index2 = indices[triangle_begin + 2u];
                if (index0 >= uv1_vertices.size() || index1 >= uv1_vertices.size() || index2 >= uv1_vertices.size() ||
                    index0 >= world_positions.size() || index1 >= world_positions.size() || index2 >= world_positions.size())
                {
                    AddMessage(out_result.errors,
                               "triangle_index_range",
                               "Atlas builder encountered a primitive index outside imported vertex ranges.");
                    continue;
                }

                const glm::fvec2 triangle_uv[3] = {
                    uv1_vertices[index0],
                    uv1_vertices[index1],
                    uv1_vertices[index2],
                };
                const glm::fvec3 triangle_positions[3] = {
                    world_positions[index0],
                    world_positions[index1],
                    world_positions[index2],
                };

                const float min_u = (std::min)((std::min)(triangle_uv[0].x, triangle_uv[1].x), triangle_uv[2].x);
                const float min_v = (std::min)((std::min)(triangle_uv[0].y, triangle_uv[1].y), triangle_uv[2].y);
                const float max_u = (std::max)((std::max)(triangle_uv[0].x, triangle_uv[1].x), triangle_uv[2].x);
                const float max_v = (std::max)((std::max)(triangle_uv[0].y, triangle_uv[1].y), triangle_uv[2].y);

                const int min_x = (std::max)(0, static_cast<int>(std::floor(min_u * static_cast<float>(settings.atlas_resolution))));
                const int min_y = (std::max)(0, static_cast<int>(std::floor(min_v * static_cast<float>(settings.atlas_resolution))));
                const int max_x = (std::min)(static_cast<int>(settings.atlas_resolution) - 1,
                                             static_cast<int>(std::ceil(max_u * static_cast<float>(settings.atlas_resolution))) - 1);
                const int max_y = (std::min)(static_cast<int>(settings.atlas_resolution) - 1,
                                             static_cast<int>(std::ceil(max_v * static_cast<float>(settings.atlas_resolution))) - 1);
                if (min_x > max_x || min_y > max_y)
                {
                    continue;
                }

                const glm::fvec3 geometric_normal_raw = glm::cross(triangle_positions[1] - triangle_positions[0],
                                                                    triangle_positions[2] - triangle_positions[0]);
                const float geometric_normal_length = glm::length(geometric_normal_raw);
                const glm::fvec3 geometric_normal = geometric_normal_length > 1e-8f
                    ? geometric_normal_raw / geometric_normal_length
                    : glm::fvec3(0.0f, 1.0f, 0.0f);

                for (int y = min_y; y <= max_y; ++y)
                {
                    for (int x = min_x; x <= max_x; ++x)
                    {
                        const glm::fvec2 sample_uv(
                            (static_cast<float>(x) + 0.5f) / static_cast<float>(settings.atlas_resolution),
                            (static_cast<float>(y) + 0.5f) / static_cast<float>(settings.atlas_resolution));

                        bool barycentric_valid = false;
                        const glm::fvec3 barycentric = ComputeBarycentric(
                            sample_uv,
                            triangle_uv[0],
                            triangle_uv[1],
                            triangle_uv[2],
                            barycentric_valid);
                        if (!barycentric_valid || !IsInsideTriangle(barycentric))
                        {
                            continue;
                        }

                        const glm::uvec2 texel_coord = ClampTexelCoord(x, y, settings.atlas_resolution);
                        const std::size_t texel_index = ToTexelIndex(texel_coord.x, texel_coord.y, settings.atlas_resolution);
                        const int current_owner = texel_owner[texel_index];
                        if (current_owner == -1)
                        {
                            texel_owner[texel_index] = static_cast<int>(binding_index);

                            LightmapAtlasTexelRecord texel_record{};
                            texel_record.atlas_id = 0u;
                            texel_record.texel_x = texel_coord.x;
                            texel_record.texel_y = texel_coord.y;
                            texel_record.stable_node_key = primitive.stable_node_key;
                            texel_record.primitive_hash = primitive.primitive_hash;
                            texel_record.triangle_index = triangle_begin / 3u;
                            texel_record.atlas_uv = sample_uv;
                            texel_record.world_position =
                                triangle_positions[0] * barycentric.x +
                                triangle_positions[1] * barycentric.y +
                                triangle_positions[2] * barycentric.z;
                            texel_record.geometric_normal = geometric_normal;
                            out_result.texel_records.push_back(std::move(texel_record));

                            ++binding.valid_texel_count;
                            ++out_result.texel_record_count;
                            if (!binding.has_texel_bounds)
                            {
                                binding.has_texel_bounds = true;
                                binding.texel_min = texel_coord;
                                binding.texel_max = texel_coord;
                            }
                            else
                            {
                                binding.texel_min = glm::min(binding.texel_min, texel_coord);
                                binding.texel_max = glm::max(binding.texel_max, texel_coord);
                            }
                            continue;
                        }

                        if (current_owner == static_cast<int>(binding_index))
                        {
                            continue;
                        }

                        if (texel_overlap_mask[texel_index] == 0u)
                        {
                            texel_overlap_mask[texel_index] = 1u;
                            ++out_result.overlapped_texel_count;
                            ++binding.overlap_texel_count;
                            ++out_result.bindings[static_cast<std::size_t>(current_owner)].overlap_texel_count;
                        }
                    }
                }
            }

            if (binding.valid_texel_count == 0u)
            {
                AddMessage(out_result.errors,
                           "empty_texel_coverage",
                           "Primitive produced no covered texels at the target atlas resolution.");
            }
        }

        out_result.binding_count = static_cast<unsigned>(out_result.bindings.size());
        if (out_result.overlapped_texel_count > 0u)
        {
            AddMessage(out_result.errors,
                       "uv1_overlap_texels",
                       "Atlas rasterization detected overlapping texel ownership between primitives.");
        }

        if (out_result.binding_count == 0u && !import_result.primitive_instances.empty())
        {
            AddMessage(out_result.warnings,
                       "no_bindings",
                       "Atlas builder did not receive any import-valid primitive bindings.");
        }

        return true;
    }
}
