#include "BakeAccumulator.h"

#include "Bake/Atlas/LightmapAtlasBuilder.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <system_error>

#include <nlohmann_json/single_include/nlohmann/json.hpp>

namespace LightingBaker
{
    namespace
    {
        constexpr unsigned kExpectedTexelRecordStride = 56u;

        std::filesystem::path NormalizePathForComparison(const std::filesystem::path& path)
        {
            if (path.empty())
            {
                return {};
            }

            std::error_code error_code{};
            const std::filesystem::path absolute_path = path.is_absolute() ? path : std::filesystem::absolute(path, error_code);
            if (!error_code)
            {
                return absolute_path.lexically_normal();
            }

            return path.lexically_normal();
        }

        bool ReadJsonFile(const std::filesystem::path& file_path, nlohmann::json& out_root, std::wstring& out_error)
        {
            std::ifstream stream(file_path, std::ios::in | std::ios::binary);
            if (!stream.is_open())
            {
                out_error = L"Failed to open resume metadata file: " + file_path.native();
                return false;
            }

            try
            {
                stream >> out_root;
            }
            catch (const std::exception& exception)
            {
                const std::string error_message = exception.what();
                out_error = L"Failed to parse resume metadata json: " + file_path.native() +
                    L" (" + std::wstring(error_message.begin(), error_message.end()) + L")";
                return false;
            }

            return true;
        }

        bool ReadUnsignedField(const nlohmann::json& root,
                               const char* field_name,
                               unsigned& out_value,
                               std::wstring& out_error)
        {
            const auto field_it = root.find(field_name);
            if (field_it == root.end() || !field_it->is_number_unsigned())
            {
                out_error = L"Resume metadata is missing an unsigned field: " + std::wstring(field_name, field_name + std::strlen(field_name));
                return false;
            }

            out_value = field_it->get<unsigned>();
            return true;
        }

        bool ReadBoolField(const nlohmann::json& root,
                           const char* field_name,
                           bool& out_value,
                           std::wstring& out_error)
        {
            const auto field_it = root.find(field_name);
            if (field_it == root.end() || !field_it->is_boolean())
            {
                out_error = L"Resume metadata is missing a boolean field: " + std::wstring(field_name, field_name + std::strlen(field_name));
                return false;
            }

            out_value = field_it->get<bool>();
            return true;
        }

        bool ReadPathField(const nlohmann::json& root,
                           const char* field_name,
                           std::filesystem::path& out_path,
                           std::wstring& out_error)
        {
            const auto field_it = root.find(field_name);
            if (field_it == root.end() || !field_it->is_string())
            {
                out_error = L"Resume metadata is missing a string path field: " + std::wstring(field_name, field_name + std::strlen(field_name));
                return false;
            }

            out_path = std::filesystem::path(field_it->get<std::string>());
            return true;
        }

        unsigned CountAtlasTexelRecords(const LightmapAtlasBuildResult& atlas_result, unsigned atlas_id)
        {
            unsigned count = 0u;
            for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
            {
                if (texel_record.atlas_id == atlas_id)
                {
                    ++count;
                }
            }

            return count;
        }

        bool ValidateExistingFileSize(const std::filesystem::path& file_path,
                                      std::uint64_t expected_size,
                                      const wchar_t* label,
                                      std::wstring& out_error)
        {
            std::error_code error_code{};
            if (!std::filesystem::exists(file_path, error_code) || error_code)
            {
                out_error = std::wstring(L"Missing ") + label + L": " + file_path.native();
                return false;
            }

            const std::uint64_t actual_size = std::filesystem::file_size(file_path, error_code);
            if (error_code)
            {
                out_error = std::wstring(L"Failed to read size of ") + label + L": " + file_path.native();
                return false;
            }

            if (actual_size != expected_size)
            {
                out_error = std::wstring(L"Unexpected size for ") + label + L": " + file_path.native();
                return false;
            }

            return true;
        }
    }

    bool BakeAccumulator::LoadResumeState(const std::filesystem::path& resume_path,
                                          BakeAccumulatorResumeState& out_state,
                                          std::wstring& out_error) const
    {
        nlohmann::json root{};
        if (!ReadJsonFile(resume_path, root, out_error))
        {
            return false;
        }

        BakeAccumulatorResumeState state{};
        state.resume_path = resume_path;
        state.cache_root = resume_path.parent_path();

        if (!ReadPathField(root, "source_scene", state.source_scene, out_error))
        {
            return false;
        }

        const auto job_it = root.find("job");
        if (job_it == root.end() || !job_it->is_object())
        {
            out_error = L"Resume metadata is missing the 'job' object.";
            return false;
        }
        if (!ReadUnsignedField(*job_it, "atlas_resolution", state.atlas_resolution, out_error) ||
            !ReadUnsignedField(*job_it, "samples_per_iteration", state.samples_per_iteration, out_error) ||
            !ReadUnsignedField(*job_it, "target_samples", state.target_samples, out_error) ||
            !ReadUnsignedField(*job_it, "max_bounces", state.max_bounces, out_error) ||
            !ReadBoolField(*job_it, "progressive", state.progressive, out_error))
        {
            return false;
        }

        const auto progress_it = root.find("progress");
        if (progress_it == root.end() || !progress_it->is_object())
        {
            out_error = L"Resume metadata is missing the 'progress' object.";
            return false;
        }
        if (!ReadUnsignedField(*progress_it, "completed_samples", state.completed_samples, out_error) ||
            !ReadBoolField(*progress_it, "has_accumulation_cache", state.has_accumulation_cache, out_error))
        {
            return false;
        }

        const auto atlas_inputs_it = root.find("atlas_inputs");
        if (atlas_inputs_it == root.end() || !atlas_inputs_it->is_array())
        {
            out_error = L"Resume metadata is missing the 'atlas_inputs' array.";
            return false;
        }

        for (const auto& atlas_input_json : *atlas_inputs_it)
        {
            if (!atlas_input_json.is_object())
            {
                out_error = L"Resume metadata contains a malformed atlas input entry.";
                return false;
            }

            BakeAccumulatorAtlasInputState atlas_input{};
            if (!ReadUnsignedField(atlas_input_json, "atlas_id", atlas_input.atlas_id, out_error) ||
                !ReadUnsignedField(atlas_input_json, "texel_record_count", atlas_input.texel_record_count, out_error) ||
                !ReadUnsignedField(atlas_input_json, "texel_record_stride", atlas_input.texel_record_stride, out_error) ||
                !ReadPathField(atlas_input_json, "texel_record_file", atlas_input.texel_record_file, out_error) ||
                !ReadPathField(atlas_input_json, "accumulation_file", atlas_input.accumulation_file, out_error) ||
                !ReadPathField(atlas_input_json, "sample_count_file", atlas_input.sample_count_file, out_error) ||
                !ReadPathField(atlas_input_json, "variance_file", atlas_input.variance_file, out_error))
            {
                return false;
            }

            state.atlas_inputs.push_back(std::move(atlas_input));
        }

        out_state = std::move(state);
        return true;
    }

    bool BakeAccumulator::ValidateResumeState(const BakeAccumulatorResumeState& resume_state,
                                              const BakeJobConfig& config,
                                              const LightmapAtlasBuildResult& atlas_result,
                                              std::wstring& out_error) const
    {
        if (NormalizePathForComparison(resume_state.source_scene) != NormalizePathForComparison(config.scene_path))
        {
            out_error = L"Resume cache scene path does not match the requested bake scene.";
            return false;
        }

        if (resume_state.atlas_resolution != config.atlas_resolution)
        {
            out_error = L"Resume cache atlas resolution does not match the requested job.";
            return false;
        }
        if (resume_state.samples_per_iteration != config.samples_per_iteration)
        {
            out_error = L"Resume cache samples-per-iteration does not match the requested job.";
            return false;
        }
        if (resume_state.target_samples != config.target_samples)
        {
            out_error = L"Resume cache target sample count does not match the requested job.";
            return false;
        }
        if (resume_state.max_bounces != config.max_bounces)
        {
            out_error = L"Resume cache max-bounces does not match the requested job.";
            return false;
        }
        if (resume_state.progressive != config.progressive)
        {
            out_error = L"Resume cache progressive mode does not match the requested job.";
            return false;
        }
        if (resume_state.completed_samples > resume_state.target_samples)
        {
            out_error = L"Resume cache has an invalid completed sample count.";
            return false;
        }
        if (resume_state.atlas_inputs.empty())
        {
            out_error = L"Resume cache does not contain any atlas input entries.";
            return false;
        }

        for (const BakeAccumulatorAtlasInputState& atlas_input : resume_state.atlas_inputs)
        {
            const unsigned expected_texel_record_count = CountAtlasTexelRecords(atlas_result, atlas_input.atlas_id);
            if (atlas_input.texel_record_count != expected_texel_record_count)
            {
                out_error = L"Resume cache texel record count does not match the current atlas build.";
                return false;
            }
            if (atlas_input.texel_record_stride != kExpectedTexelRecordStride)
            {
                out_error = L"Resume cache texel record stride does not match the supported disk layout.";
                return false;
            }

            const std::filesystem::path texel_record_path = resume_state.cache_root / atlas_input.texel_record_file;
            if (!ValidateExistingFileSize(texel_record_path,
                                          static_cast<std::uint64_t>(atlas_input.texel_record_count) * atlas_input.texel_record_stride,
                                          L"texel record cache file",
                                          out_error))
            {
                return false;
            }

            const std::filesystem::path accumulation_path = resume_state.cache_root / atlas_input.accumulation_file;
            if (!ValidateExistingFileSize(accumulation_path,
                                          static_cast<std::uint64_t>(atlas_input.texel_record_count) * 3ull * sizeof(float),
                                          L"accumulation cache file",
                                          out_error))
            {
                return false;
            }

            const std::filesystem::path sample_count_path = resume_state.cache_root / atlas_input.sample_count_file;
            if (!ValidateExistingFileSize(sample_count_path,
                                          static_cast<std::uint64_t>(atlas_input.texel_record_count) * sizeof(std::uint32_t),
                                          L"sample count cache file",
                                          out_error))
            {
                return false;
            }

            const std::filesystem::path variance_path = resume_state.cache_root / atlas_input.variance_file;
            if (!ValidateExistingFileSize(variance_path,
                                          static_cast<std::uint64_t>(atlas_input.texel_record_count) * sizeof(float),
                                          L"variance cache file",
                                          out_error))
            {
                return false;
            }
        }

        return true;
    }
}
