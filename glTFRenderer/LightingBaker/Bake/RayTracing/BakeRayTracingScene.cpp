#include "BakeRayTracingScene.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
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
        std::map<std::string, std::uint32_t> material_texture_index_by_uri{};
        const auto register_material_texture =
            [&material_texture_index_by_uri, &out_result](const std::string& texture_uri) -> std::uint32_t
        {
            const auto [texture_it, inserted] =
                material_texture_index_by_uri.emplace(
                    texture_uri,
                    static_cast<std::uint32_t>(out_result.material_texture_uris.size()));
            if (inserted)
            {
                out_result.material_texture_uris.push_back(texture_uri);
            }
            return texture_it->second;
        };

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
            geometry_source.opaque = !primitive.material.alpha_blended;
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

            if (primitive.material.alpha_masked)
            {
                out_result.warnings.push_back(MakeMessage(
                    "rt_scene_alpha_mask_fallback",
                    "Alpha-masked materials are currently baked as opaque geometry in the ray tracing scene."));
            }

            if (primitive.geometry.world_normals.size() != primitive.geometry.world_positions.size())
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_scene_normal_count_mismatch",
                    "Primitive shading normals do not match imported position count."));
                ++out_result.skipped_primitive_count;
                continue;
            }
            if (primitive.geometry.uv1_vertices.size() != primitive.geometry.world_positions.size())
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_scene_uv1_count_mismatch",
                    "Primitive bake texcoords do not match imported position count."));
                ++out_result.skipped_primitive_count;
                continue;
            }

            const std::size_t shading_vertex_offset = out_result.shading_vertices.size();
            const std::size_t shading_index_offset = out_result.shading_indices.size();
            if (shading_vertex_offset >
                    static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
                shading_index_offset >
                    static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
                geometry_source.vertex_count >
                    static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) - shading_vertex_offset ||
                geometry_source.index_count >
                    static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) - shading_index_offset)
            {
                out_result.errors.push_back(MakeMessage(
                    "rt_scene_shading_offset_overflow",
                    "Scene shading buffer offsets exceeded 32-bit addressable range."));
                ++out_result.skipped_primitive_count;
                continue;
            }

            out_result.shading_vertices.reserve(out_result.shading_vertices.size() + geometry_source.vertex_count);
            for (std::size_t vertex_index = 0; vertex_index < primitive.geometry.world_positions.size(); ++vertex_index)
            {
                const glm::fvec3& world_position = primitive.geometry.world_positions[vertex_index];
                const glm::fvec3& world_normal = primitive.geometry.world_normals[vertex_index];
                const glm::fvec2 texcoord0 =
                    primitive.geometry.uv0_vertices.size() == primitive.geometry.world_positions.size()
                        ? primitive.geometry.uv0_vertices[vertex_index]
                        : glm::fvec2(0.0f, 0.0f);
                const glm::fvec2& texcoord1 = primitive.geometry.uv1_vertices[vertex_index];
                out_result.shading_vertices.push_back({
                    .world_position = {world_position.x, world_position.y, world_position.z, 1.0f},
                    .world_normal = {world_normal.x, world_normal.y, world_normal.z, 0.0f},
                    .texcoord0_texcoord1 = {texcoord0.x, texcoord0.y, texcoord1.x, texcoord1.y},
                });
            }

            out_result.shading_indices.reserve(out_result.shading_indices.size() + geometry_source.index_count);
            for (const unsigned index : primitive.geometry.triangle_indices)
            {
                out_result.shading_indices.push_back(static_cast<std::uint32_t>(index));
            }

            std::uint32_t base_color_texture_index = BakeRayTracingSceneTextureInvalidIndex;
            if (primitive.material.has_base_color_texture && !primitive.material.base_color_texture_uri.empty())
            {
                base_color_texture_index = register_material_texture(primitive.material.base_color_texture_uri);
            }

            std::uint32_t emissive_texture_index = BakeRayTracingSceneTextureInvalidIndex;
            if (primitive.material.has_emissive_texture && !primitive.material.emissive_texture_uri.empty())
            {
                emissive_texture_index = register_material_texture(primitive.material.emissive_texture_uri);
            }

            BakeRayTracingSceneInstanceGPU shading_instance{};
            shading_instance.offsets_and_flags = {
                static_cast<std::uint32_t>(shading_vertex_offset),
                static_cast<std::uint32_t>(shading_index_offset),
                primitive.material.material_index,
                primitive.material.double_sided ? BakeRayTracingSceneInstanceFlagDoubleSided : 0u,
            };
            shading_instance.texture_indices_and_texcoords = {
                base_color_texture_index,
                primitive.material.base_color_texture_texcoord,
                emissive_texture_index,
                primitive.material.emissive_texture_texcoord,
            };
            shading_instance.base_color = {
                primitive.material.base_color_factor.x,
                primitive.material.base_color_factor.y,
                primitive.material.base_color_factor.z,
                primitive.material.base_color_factor.w,
            };
            shading_instance.emissive_and_roughness = {
                primitive.material.emissive_factor.x,
                primitive.material.emissive_factor.y,
                primitive.material.emissive_factor.z,
                primitive.material.roughness_factor,
            };
            shading_instance.metallic_and_padding = {
                primitive.material.metallic_factor,
                0.0f,
                0.0f,
                0.0f,
            };

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
            out_result.shading_instances.push_back(shading_instance);
            out_result.geometries.push_back(std::move(geometry_source));
        }

        out_result.geometry_count = out_result.geometries.size();
        out_result.instance_count = out_result.instances.size();
        out_result.shading_vertex_count = out_result.shading_vertices.size();
        out_result.shading_index_count = out_result.shading_indices.size();
        out_result.shading_instance_count = out_result.shading_instances.size();
        out_result.material_texture_count = out_result.material_texture_uris.size();

        if (out_result.geometry_count == 0u)
        {
            out_result.errors.push_back(MakeMessage(
                "rt_scene_empty",
                "Ray tracing scene builder did not receive any import-valid lightmap primitives."));
        }

        return true;
    }
}
