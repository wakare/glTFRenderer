#pragma once

#include "Scene/BakeSceneImporter.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "RHICommon.h"
#include "RHIInterface/IRHIRayTracingAS.h"

namespace LightingBaker
{
    constexpr unsigned BakeRayTracingSceneInstanceFlagDoubleSided = 1u << 0u;
    constexpr unsigned BakeRayTracingSceneTextureInvalidIndex = 0xffffffffu;

    struct BakeRayTracingSceneVertexGPU
    {
        std::array<float, 4u> world_position{0.0f, 0.0f, 0.0f, 1.0f};
        std::array<float, 4u> world_normal{0.0f, 1.0f, 0.0f, 0.0f};
        std::array<float, 4u> texcoord0_texcoord1{0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct BakeRayTracingSceneInstanceGPU
    {
        std::array<std::uint32_t, 4u> offsets_and_flags{0u, 0u, 0u, 0u};
        std::array<std::uint32_t, 4u> texture_indices_and_texcoords{
            BakeRayTracingSceneTextureInvalidIndex,
            0u,
            BakeRayTracingSceneTextureInvalidIndex,
            0u};
        std::array<float, 4u> base_color{1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 4u> emissive_and_roughness{0.0f, 0.0f, 0.0f, 1.0f};
        std::array<float, 4u> metallic_and_padding{1.0f, 0.0f, 0.0f, 0.0f};
    };

    struct BakeRayTracingGeometrySource
    {
        unsigned geometry_index{0u};
        unsigned stable_node_key{0xffffffffu};
        unsigned primitive_hash{0xffffffffu};
        unsigned material_index{0xffffffffu};
        std::size_t vertex_count{0u};
        std::size_t index_count{0u};
        std::size_t triangle_count{0u};
        bool opaque{true};
        std::string node_name{};
        std::string mesh_name{};
        VertexBufferData vertex_buffer_data{};
        IndexBufferData index_buffer_data{};
    };

    struct BakeRayTracingSceneBuildResult
    {
        std::size_t geometry_count{0u};
        std::size_t instance_count{0u};
        std::size_t vertex_count{0u};
        std::size_t index_count{0u};
        std::size_t triangle_count{0u};
        std::size_t shading_vertex_count{0u};
        std::size_t shading_index_count{0u};
        std::size_t shading_instance_count{0u};
        std::size_t material_texture_count{0u};
        std::size_t skipped_primitive_count{0u};
        std::vector<BakeRayTracingGeometrySource> geometries{};
        std::vector<RHIRayTracingInstanceDesc> instances{};
        std::vector<BakeRayTracingSceneVertexGPU> shading_vertices{};
        std::vector<std::uint32_t> shading_indices{};
        std::vector<BakeRayTracingSceneInstanceGPU> shading_instances{};
        std::vector<std::string> material_texture_uris{};
        std::vector<BakeSceneValidationMessage> errors{};
        std::vector<BakeSceneValidationMessage> warnings{};

        bool HasErrors() const;
        bool HasValidationErrors() const;
    };

    class BakeRayTracingSceneBuilder
    {
    public:
        bool BuildScene(const BakeSceneImportResult& import_result,
                        BakeRayTracingSceneBuildResult& out_result,
                        std::wstring& out_error) const;
    };
}
