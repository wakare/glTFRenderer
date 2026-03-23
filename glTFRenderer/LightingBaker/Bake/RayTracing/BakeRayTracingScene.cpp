#include "BakeRayTracingScene.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

#include <glm/glm/glm.hpp>

namespace LightingBaker
{
    namespace
    {
        constexpr std::array<float, 12> kIdentityTransform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f};

        BakeSceneValidationMessage MakeMessage(std::string code, std::string message)
        {
            return {
                .code = std::move(code),
                .message = std::move(message)
            };
        }

        VertexLayoutDeclaration BuildPositionOnlyLayout()
        {
            VertexLayoutDeclaration layout{};
            layout.elements.push_back({
                .type = VertexAttributeType::VERTEX_POSITION,
                .byte_size = static_cast<unsigned>(sizeof(glm::fvec3))
            });
            return layout;
        }
    }

    bool BakeRayTracingSceneBuildResult::HasErrors() const
    {
        return !errors.empty();
    }

    bool BakeRayTracingSceneBuildResult::HasValidationErrors() const
    {
        return HasErrors();
    }

    bool BakeRayTracingSceneBuilder::BuildScene(const BakeSceneImportResult& import_result,
                                                BakeRayTracingSceneBuildResult& out_result,
                                                std::wstring& out_error) const
    {
        out_result = BakeRayTracingSceneBuildResult{};
        out_error.clear();

        if (!import_result.load_succeeded)
        {
            out_error = L"Ray tracing scene build requires a successfully imported scene.";
            return false;
        }

        const VertexLayoutDeclaration position_only_layout = BuildPositionOnlyLayout();

        for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
        {
            if (!primitive.can_emit_lightmap_binding)
            {
                ++out_result.skipped_primitive_count;
                continue;
            }

            if (!primitive.geometry.HasGeometry())
            {
                out_result.warnings.push_back(MakeMessage(
                    "rt_scene_missing_geometry",
                    "Skipping primitive without valid world-space bake geometry."));
                ++out_result.skipped_primitive_count;
                continue;
            }

            if ((primitive.geometry.triangle_indices.size() % 3u) != 0u)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_scene_invalid_index_count",
                    "Primitive triangle index count is not divisible by three."));
                ++out_result.skipped_primitive_count;
                continue;
            }

            if (primitive.geometry.world_positions.size() >
                    static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
                primitive.geometry.triangle_indices.size() >
                    static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()))
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_scene_index_range_overflow",
                    "Primitive geometry exceeds 32-bit ray tracing scene limits."));
                ++out_result.skipped_primitive_count;
                continue;
            }

            BakeRayTracingGeometrySource geometry_source{};
            geometry_source.geometry_index = static_cast<unsigned>(out_result.geometries.size());
            geometry_source.stable_node_key = primitive.stable_node_key;
            geometry_source.primitive_hash = primitive.primitive_hash;
            geometry_source.material_index = primitive.material_index;
            geometry_source.vertex_count = primitive.geometry.world_positions.size();
            geometry_source.index_count = primitive.geometry.triangle_indices.size();
            geometry_source.triangle_count = primitive.geometry.triangle_indices.size() / 3u;
            geometry_source.opaque = true;
            geometry_source.node_name = primitive.node_name;
            geometry_source.mesh_name = primitive.mesh_name;

            geometry_source.vertex_buffer_data.layout = position_only_layout;
            geometry_source.vertex_buffer_data.vertex_count = geometry_source.vertex_count;
            geometry_source.vertex_buffer_data.byte_size =
                geometry_source.vertex_count * sizeof(glm::fvec3);
            geometry_source.vertex_buffer_data.data =
                std::make_unique<char[]>(geometry_source.vertex_buffer_data.byte_size);
            std::memcpy(
                geometry_source.vertex_buffer_data.data.get(),
                primitive.geometry.world_positions.data(),
                geometry_source.vertex_buffer_data.byte_size);

            geometry_source.index_buffer_data.format = RHIDataFormat::R32_UINT;
            geometry_source.index_buffer_data.index_count = geometry_source.index_count;
            geometry_source.index_buffer_data.byte_size =
                geometry_source.index_count * sizeof(unsigned);
            geometry_source.index_buffer_data.data =
                std::make_unique<char[]>(geometry_source.index_buffer_data.byte_size);
            std::memcpy(
                geometry_source.index_buffer_data.data.get(),
                primitive.geometry.triangle_indices.data(),
                geometry_source.index_buffer_data.byte_size);

            RHIRayTracingInstanceDesc instance_desc{};
            std::copy(kIdentityTransform.begin(), kIdentityTransform.end(), instance_desc.transform.begin());
            instance_desc.instance_id = static_cast<std::uint32_t>(out_result.instances.size());
            instance_desc.instance_mask = 0xFFu;
            instance_desc.hit_group_index = geometry_source.geometry_index;
            instance_desc.geometry_index = geometry_source.geometry_index;

            out_result.vertex_count += geometry_source.vertex_count;
            out_result.index_count += geometry_source.index_count;
            out_result.triangle_count += geometry_source.triangle_count;
            out_result.instances.push_back(instance_desc);
            out_result.geometries.push_back(std::move(geometry_source));
        }

        out_result.geometry_count = out_result.geometries.size();
        out_result.instance_count = out_result.instances.size();

        if (out_result.geometry_count == 0u)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_scene_empty",
                "Ray tracing scene builder did not receive any import-valid lightmap primitives."));
        }

        return true;
    }
}
