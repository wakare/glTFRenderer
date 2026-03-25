#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm/glm.hpp>

namespace LightingBaker
{
    enum class BakeSceneLightType : unsigned
    {
        Directional = 0u,
        Point = 1u,
        Spot = 2u,
    };

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
        std::vector<glm::fvec3> world_normals{};
        std::vector<glm::fvec2> uv0_vertices{};
        std::vector<glm::fvec2> uv1_vertices{};
        std::vector<unsigned> triangle_indices{};

        bool HasGeometry() const
        {
            return !world_positions.empty() &&
                   world_positions.size() == world_normals.size() &&
                   world_positions.size() == uv1_vertices.size() &&
                   !triangle_indices.empty();
        }
    };

    struct BakeMaterialImportInfo
    {
        unsigned material_index{0xffffffffu};
        glm::fvec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
        glm::fvec3 emissive_factor{0.0f, 0.0f, 0.0f};
        float metallic_factor{1.0f};
        float roughness_factor{1.0f};
        float alpha_cutoff{0.5f};
        unsigned base_color_texture_texcoord{0u};
        bool has_base_color_texture{false};
        unsigned emissive_texture_texcoord{0u};
        bool has_emissive_texture{false};
        bool double_sided{false};
        bool alpha_masked{false};
        bool alpha_blended{false};
        std::string base_color_texture_uri{};
        std::string emissive_texture_uri{};
    };

    struct BakeSceneLightImportInfo
    {
        unsigned light_index{0xffffffffu};
        unsigned stable_node_key{0xffffffffu};
        BakeSceneLightType type{BakeSceneLightType::Point};
        std::string light_name{};
        std::string node_name{};
        glm::fvec3 color{1.0f, 1.0f, 1.0f};
        float intensity{1.0f};
        float range{-1.0f};
        glm::fvec3 world_position{0.0f, 0.0f, 0.0f};
        glm::fvec3 world_direction{0.0f, 0.0f, -1.0f};
        float spot_inner_cone_angle{0.0f};
        float spot_outer_cone_angle{0.78539816339f};
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
        bool has_normals{false};
        BakePrimitiveGeometryData geometry{};
        BakeMaterialImportInfo material{};
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
        unsigned punctual_light_count{0};
        unsigned directional_light_count{0};
        unsigned point_light_count{0};
        unsigned spot_light_count{0};
        std::vector<BakePrimitiveImportInfo> primitive_instances{};
        std::vector<BakeSceneLightImportInfo> punctual_lights{};
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
