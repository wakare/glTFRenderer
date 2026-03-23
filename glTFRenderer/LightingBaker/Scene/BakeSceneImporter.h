#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm/glm.hpp>

namespace LightingBaker
{
    struct BakeSceneImportRequest
    {
        std::filesystem::path scene_path;
    };

    struct BakeSceneValidationMessage
    {
        std::string code{};
        std::string message{};
    };

    struct BakePrimitiveGeometryData
    {
        std::vector<glm::fvec3> world_positions{};
        std::vector<glm::fvec2> uv1_vertices{};
        std::vector<unsigned> triangle_indices{};

        bool HasGeometry() const
        {
            return !world_positions.empty() &&
                   world_positions.size() == uv1_vertices.size() &&
                   !triangle_indices.empty();
        }
    };

    struct BakePrimitiveImportInfo
    {
        unsigned stable_node_key{0xffffffffu};
        unsigned primitive_hash{0xffffffffu};
        unsigned mesh_index{0xffffffffu};
        unsigned primitive_index{0xffffffffu};
        unsigned material_index{0xffffffffu};
        std::string node_name{};
        std::string mesh_name{};
        unsigned vertex_count{0};
        unsigned index_count{0};
        bool has_texcoord0{false};
        bool has_texcoord1{false};
        bool can_emit_lightmap_binding{false};
        glm::fvec2 uv1_min{0.0f, 0.0f};
        glm::fvec2 uv1_max{0.0f, 0.0f};
        unsigned uv1_out_of_range_vertex_count{0};
        unsigned uv1_non_finite_vertex_count{0};
        unsigned degenerate_uv_triangle_count{0};
        unsigned degenerate_position_triangle_count{0};
        BakePrimitiveGeometryData geometry{};
        std::vector<BakeSceneValidationMessage> errors{};
        std::vector<BakeSceneValidationMessage> warnings{};
    };

    struct BakeSceneImportResult
    {
        std::filesystem::path scene_path{};
        bool load_succeeded{false};
        unsigned node_count{0};
        unsigned mesh_count{0};
        unsigned instance_primitive_count{0};
        unsigned valid_lightmap_primitive_count{0};
        std::vector<BakePrimitiveImportInfo> primitive_instances{};
        std::vector<BakeSceneValidationMessage> errors{};
        std::vector<BakeSceneValidationMessage> warnings{};

        bool HasErrors() const;
        bool HasValidationErrors() const;
    };

    class BakeSceneImporter
    {
    public:
        bool ImportScene(const BakeSceneImportRequest& request,
                         BakeSceneImportResult& out_result,
                         std::wstring& out_error) const;
    };
}
