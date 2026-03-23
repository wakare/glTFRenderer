#pragma once

#include "Scene/BakeSceneImporter.h"

#include <array>
#include <string>
#include <vector>

#include <glm/glm/glm.hpp>

namespace LightingBaker
{
    struct LightmapAtlasBuildSettings
    {
        unsigned atlas_resolution = 1024;
        unsigned texel_border = 4;
    };

    struct LightmapAtlasTexelRecord
    {
        unsigned atlas_id{0};
        unsigned texel_x{0};
        unsigned texel_y{0};
        unsigned stable_node_key{0xffffffffu};
        unsigned primitive_hash{0xffffffffu};
        unsigned triangle_index{0};
        glm::fvec2 atlas_uv{0.0f, 0.0f};
        glm::fvec3 world_position{0.0f, 0.0f, 0.0f};
        glm::fvec3 geometric_normal{0.0f, 1.0f, 0.0f};
    };

    struct LightmapAtlasBindingInfo
    {
        unsigned atlas_id{0};
        unsigned stable_node_key{0xffffffffu};
        unsigned primitive_hash{0xffffffffu};
        std::array<float, 4> scale_bias{1.0f, 1.0f, 0.0f, 0.0f};
        unsigned valid_texel_count{0};
        unsigned overlap_texel_count{0};
        bool has_texel_bounds{false};
        glm::uvec2 texel_min{0u, 0u};
        glm::uvec2 texel_max{0u, 0u};
        std::string node_name{};
        std::string mesh_name{};
    };

    struct LightmapAtlasBuildResult
    {
        unsigned atlas_resolution{0};
        unsigned texel_border{0};
        unsigned binding_count{0};
        unsigned texel_record_count{0};
        unsigned overlapped_texel_count{0};
        std::vector<LightmapAtlasBindingInfo> bindings{};
        std::vector<LightmapAtlasTexelRecord> texel_records{};
        std::vector<BakeSceneValidationMessage> errors{};
        std::vector<BakeSceneValidationMessage> warnings{};

        bool HasErrors() const;
        bool HasValidationErrors() const;
    };

    class LightmapAtlasBuilder
    {
    public:
        bool BuildAtlas(const BakeSceneImportResult& import_result,
                        const LightmapAtlasBuildSettings& settings,
                        LightmapAtlasBuildResult& out_result,
                        std::wstring& out_error) const;
    };
}
