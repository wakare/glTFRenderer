#include "BakeSceneImporter.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

#include "SceneFileLoader/glTFLoader.h"

namespace LightingBaker
{
    namespace
    {
        constexpr unsigned kInvalidUnsigned = 0xffffffffu;
        using glTFAccessor = glTF_Element_Template<glTF_Element_Type::EAccessor>;

        struct AccessorDataView
        {
            const glTF_Element_Accessor_Base* accessor{nullptr};
            const std::uint8_t* data{nullptr};
            std::size_t stride{0};
            std::size_t count{0};
        };

        void AddMessage(std::vector<BakeSceneValidationMessage>& out_messages,
                        std::string code,
                        std::string message)
        {
            out_messages.push_back(BakeSceneValidationMessage{
                .code = std::move(code),
                .message = std::move(message),
            });
        }

        void AddError(BakeSceneImportResult& out_result, std::string code, std::string message)
        {
            AddMessage(out_result.errors, std::move(code), std::move(message));
        }

        void AddWarning(BakeSceneImportResult& out_result, std::string code, std::string message)
        {
            AddMessage(out_result.warnings, std::move(code), std::move(message));
        }

        void AddError(BakePrimitiveImportInfo& out_primitive, std::string code, std::string message)
        {
            AddMessage(out_primitive.errors, std::move(code), std::move(message));
        }

        void AddWarning(BakePrimitiveImportInfo& out_primitive, std::string code, std::string message)
        {
            AddMessage(out_primitive.warnings, std::move(code), std::move(message));
        }

        std::string ResolveNodeName(const glTF_Element_Node& node, unsigned node_index)
        {
            return node.name.empty() ? ("node_" + std::to_string(node_index)) : node.name;
        }

        std::string ResolveMeshName(const glTF_Element_Mesh& mesh, unsigned mesh_index)
        {
            return mesh.name.empty() ? ("mesh_" + std::to_string(mesh_index)) : mesh.name;
        }

        bool ResolveAccessorDataView(const glTFLoader& loader,
                                     const glTFHandle& accessor_handle,
                                     AccessorDataView& out_view,
                                     std::string& out_error)
        {
            if (!accessor_handle.IsValid())
            {
                out_error = "Accessor handle is invalid.";
                return false;
            }

            const unsigned accessor_index = static_cast<unsigned>(loader.ResolveIndex(accessor_handle));
            const auto& accessors = loader.GetAccessors();
            const auto& buffer_views = loader.GetBufferViews();
            const auto& buffer_data = loader.GetBufferData();
            if (accessor_index >= accessors.size())
            {
                out_error = "Accessor index is out of range.";
                return false;
            }

            const glTF_Element_Accessor_Base& accessor = *accessors[accessor_index];
            if (!accessor.buffer_view.IsValid())
            {
                out_error = "Accessor does not reference a buffer view.";
                return false;
            }

            const unsigned buffer_view_index = static_cast<unsigned>(loader.ResolveIndex(accessor.buffer_view));
            if (buffer_view_index >= buffer_views.size())
            {
                out_error = "Buffer view index is out of range.";
                return false;
            }

            const glTF_Element_BufferView& buffer_view = *buffer_views[buffer_view_index];
            glTFHandle resolved_buffer_handle = buffer_view.buffer;
            resolved_buffer_handle.node_index = loader.ResolveIndex(buffer_view.buffer);
            const auto buffer_it = buffer_data.find(resolved_buffer_handle);
            if (buffer_it == buffer_data.end())
            {
                out_error = "Buffer data for accessor was not found.";
                return false;
            }

            out_view.accessor = &accessor;
            out_view.count = accessor.count;
            out_view.stride = buffer_view.byte_stride != 0 ? buffer_view.byte_stride : accessor.GetElementByteSize();
            out_view.data = reinterpret_cast<const std::uint8_t*>(buffer_it->second.get()) +
                            buffer_view.byte_offset + accessor.byte_offset;
            return true;
        }

        bool ReadFloat2(const AccessorDataView& view, std::size_t index, glm::fvec2& out_value)
        {
            if (!view.accessor || index >= view.count)
            {
                return false;
            }

            if (view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec2)
            {
                return false;
            }

            float components[2]{};
            std::memcpy(components, view.data + index * view.stride, sizeof(components));
            out_value = glm::fvec2(components[0], components[1]);
            return true;
        }

        bool ReadFloat3(const AccessorDataView& view, std::size_t index, glm::fvec3& out_value)
        {
            if (!view.accessor || index >= view.count)
            {
                return false;
            }

            if (view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec3)
            {
                return false;
            }

            float components[3]{};
            std::memcpy(components, view.data + index * view.stride, sizeof(components));
            out_value = glm::fvec3(components[0], components[1], components[2]);
            return true;
        }

        bool ReadUnsignedIndex(const AccessorDataView& view, std::size_t index, unsigned& out_value)
        {
            if (!view.accessor || index >= view.count ||
                view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EScalar)
            {
                return false;
            }

            const std::uint8_t* address = view.data + index * view.stride;
            switch (view.accessor->component_type)
            {
            case glTFAccessor::glTF_Accessor_Component_Type::EUnsignedByte:
            {
                std::uint8_t value = 0;
                std::memcpy(&value, address, sizeof(value));
                out_value = value;
                return true;
            }
            case glTFAccessor::glTF_Accessor_Component_Type::EUnsignedShort:
            {
                std::uint16_t value = 0;
                std::memcpy(&value, address, sizeof(value));
                out_value = value;
                return true;
            }
            case glTFAccessor::glTF_Accessor_Component_Type::EUnsignedInt:
            {
                std::uint32_t value = 0;
                std::memcpy(&value, address, sizeof(value));
                out_value = value;
                return true;
            }
            default:
                return false;
            }
        }

        glm::fvec3 TransformPoint(const glm::fmat4x4& transform, const glm::fvec3& position)
        {
            const glm::fvec4 world_position = transform * glm::fvec4(position, 1.0f);
            return glm::fvec3(world_position.x, world_position.y, world_position.z);
        }

        glm::fvec3 TransformNormal(const glm::fmat4x4& transform, const glm::fvec3& normal)
        {
            const glm::fmat3 normal_matrix = glm::transpose(glm::inverse(glm::fmat3(transform)));
            const glm::fvec3 transformed = normal_matrix * normal;
            const float transformed_length = glm::length(transformed);
            if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) ||
                transformed_length <= 1e-8f)
            {
                return glm::fvec3(0.0f, 0.0f, 0.0f);
            }

            return transformed / transformed_length;
        }

        bool ResolveTextureUri(const glTFLoader& loader,
                               const glTF_TextureInfo_Base& texture_info,
                               std::string& out_texture_uri,
                               std::string& out_error)
        {
            out_texture_uri.clear();
            out_error.clear();
            if (!texture_info.index.IsValid())
            {
                out_error = "Texture handle is invalid.";
                return false;
            }

            const auto& textures = loader.GetTextures();
            const unsigned texture_index = static_cast<unsigned>(loader.ResolveIndex(texture_info.index));
            if (texture_index >= textures.size())
            {
                out_error = "Texture index is out of range.";
                return false;
            }

            const auto& texture = *textures[texture_index];
            if (!texture.source.IsValid())
            {
                out_error = "Texture source handle is invalid.";
                return false;
            }

            const auto& images = loader.GetImages();
            const unsigned image_index = static_cast<unsigned>(loader.ResolveIndex(texture.source));
            if (image_index >= images.size())
            {
                out_error = "Texture image index is out of range.";
                return false;
            }

            const auto& image = *images[image_index];
            if (image.uri.empty())
            {
                out_error = "Embedded or buffer-view-backed images are not supported yet.";
                return false;
            }

            const std::filesystem::path texture_path =
                (std::filesystem::path(loader.GetSceneFileDirectory()) / std::filesystem::path(image.uri)).lexically_normal();
            out_texture_uri = texture_path.string();
            return true;
        }

        bool ResolveSupportedMaterialTexture(const glTFLoader& loader,
                                             BakePrimitiveImportInfo& primitive,
                                             const glTF_TextureInfo_Base& texture_info,
                                             const char* texture_label,
                                             unsigned& out_texcoord_index,
                                             bool& out_has_texture,
                                             std::string& out_texture_uri)
        {
            out_texcoord_index = 0u;
            out_has_texture = false;
            out_texture_uri.clear();
            if (!texture_info.index.IsValid())
            {
                return true;
            }

            out_texcoord_index = texture_info.texCoord_index;
            if (texture_info.texCoord_index > 1u)
            {
                AddWarning(primitive,
                           std::string(texture_label) + "_texcoord",
                           std::string(texture_label) + " uses TEXCOORD index > 1, which is not supported yet. Falling back to factor-only shading.");
                return true;
            }

            std::string texture_uri{};
            std::string texture_error{};
            if (!ResolveTextureUri(loader, texture_info, texture_uri, texture_error))
            {
                AddWarning(primitive,
                           std::string(texture_label) + "_source",
                           "Failed to resolve " + std::string(texture_label) + " for the baker: " + texture_error);
                return true;
            }

            if (!std::filesystem::exists(texture_uri))
            {
                AddWarning(primitive,
                           std::string(texture_label) + "_missing",
                           "Resolved " + std::string(texture_label) + " file does not exist on disk. Falling back to factor-only shading.");
                return true;
            }

            out_has_texture = true;
            out_texture_uri = std::move(texture_uri);
            return true;
        }

        void PopulatePrimitiveMaterial(const glTFLoader& loader,
                                       BakePrimitiveImportInfo& primitive,
                                       BakeMaterialImportInfo& out_material)
        {
            out_material = BakeMaterialImportInfo{};
            out_material.material_index = primitive.material_index;

            if (primitive.material_index == kInvalidUnsigned)
            {
                return;
            }

            const auto& materials = loader.GetMaterials();
            if (primitive.material_index >= materials.size() || !materials[primitive.material_index])
            {
                return;
            }

            const glTF_Element_Material& material = *materials[primitive.material_index];
            out_material.base_color_factor = material.pbr.base_color_factor;
            out_material.emissive_factor = material.emissive_factor;
            out_material.metallic_factor = material.pbr.metallic_factor;
            out_material.roughness_factor = material.pbr.roughness_factor;
            out_material.double_sided = material.double_sided;
            out_material.alpha_masked = material.alpha_mode == "MASK";
            out_material.alpha_blended = material.alpha_mode == "BLEND";
            ResolveSupportedMaterialTexture(loader,
                                            primitive,
                                            material.pbr.base_color_texture,
                                            "baseColorTexture",
                                            out_material.base_color_texture_texcoord,
                                            out_material.has_base_color_texture,
                                            out_material.base_color_texture_uri);
            ResolveSupportedMaterialTexture(loader,
                                            primitive,
                                            material.emissive_texture,
                                            "emissiveTexture",
                                            out_material.emissive_texture_texcoord,
                                            out_material.has_emissive_texture,
                                            out_material.emissive_texture_uri);
        }

        void ValidatePrimitiveGeometry(const glTFLoader& loader,
                                       const glTF_Primitive& primitive,
                                       const glm::fmat4x4& world_transform,
                                       BakePrimitiveImportInfo& out_primitive)
        {
            const auto it_position = primitive.attributes.find(glTF_Attribute_POSITION::attribute_type_id);
            const auto it_normal = primitive.attributes.find(glTF_Attribute_NORMAL::attribute_type_id);
            const auto it_uv0 = primitive.attributes.find(glTF_Attribute_TEXCOORD_0::attribute_type_id);
            const auto it_uv1 = primitive.attributes.find(glTF_Attribute_TEXCOORD_1::attribute_type_id);
            if (it_position == primitive.attributes.end() || it_uv1 == primitive.attributes.end() || !primitive.indices.IsValid())
            {
                return;
            }

            AccessorDataView position_view{};
            AccessorDataView normal_view{};
            AccessorDataView uv0_view{};
            AccessorDataView uv1_view{};
            AccessorDataView index_view{};
            std::string accessor_error{};
            if (!ResolveAccessorDataView(loader, it_position->second, position_view, accessor_error))
            {
                AddError(out_primitive, "position_accessor", accessor_error);
                return;
            }
            if (!ResolveAccessorDataView(loader, it_uv1->second, uv1_view, accessor_error))
            {
                AddError(out_primitive, "uv1_accessor", accessor_error);
                return;
            }
            if (!ResolveAccessorDataView(loader, primitive.indices, index_view, accessor_error))
            {
                AddError(out_primitive, "index_accessor", accessor_error);
                return;
            }

            bool use_normal_accessor = false;
            bool use_uv0_accessor = false;
            if (it_normal != primitive.attributes.end())
            {
                out_primitive.has_normals = true;
                if (!ResolveAccessorDataView(loader, it_normal->second, normal_view, accessor_error))
                {
                    AddWarning(out_primitive,
                               "normal_accessor",
                               "Failed to resolve NORMAL accessor. Falling back to generated shading normals.");
                }
                else if (normal_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                         normal_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec3)
                {
                    AddWarning(out_primitive,
                               "normal_format",
                               "NORMAL accessor must be float3. Falling back to generated shading normals.");
                }
                else if (normal_view.count != position_view.count)
                {
                    AddWarning(out_primitive,
                               "normal_count_mismatch",
                               "NORMAL count does not match POSITION count. Falling back to generated shading normals.");
                }
                else
                {
                    use_normal_accessor = true;
                }
            }

            if (it_uv0 != primitive.attributes.end())
            {
                if (!ResolveAccessorDataView(loader, it_uv0->second, uv0_view, accessor_error))
                {
                    AddWarning(out_primitive,
                               "uv0_accessor",
                               "Failed to resolve TEXCOORD_0 accessor. Ignoring TEXCOORD_0 for baker shading textures.");
                }
                else if (uv0_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                         uv0_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec2)
                {
                    AddWarning(out_primitive,
                               "uv0_format",
                               "TEXCOORD_0 accessor must be float2 for baker shading textures. Ignoring TEXCOORD_0.");
                }
                else if (uv0_view.count != position_view.count)
                {
                    AddWarning(out_primitive,
                               "uv0_count_mismatch",
                               "TEXCOORD_0 count does not match POSITION count. Ignoring TEXCOORD_0 for baker shading textures.");
                }
                else
                {
                    use_uv0_accessor = true;
                }
            }

            if (position_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                position_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec3)
            {
                AddError(out_primitive, "position_format", "POSITION accessor must be float3.");
                return;
            }

            if (uv1_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                uv1_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec2)
            {
                AddError(out_primitive, "uv1_format", "TEXCOORD_1 accessor must be float2.");
                return;
            }

            if (position_view.count == 0)
            {
                AddError(out_primitive, "vertex_count", "Primitive has zero vertices.");
                return;
            }

            if (uv1_view.count != position_view.count)
            {
                AddError(out_primitive, "uv1_count_mismatch", "TEXCOORD_1 count does not match POSITION count.");
                return;
            }

            if (index_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EScalar)
            {
                AddError(out_primitive, "index_format", "Index accessor must be scalar.");
                return;
            }

            if (index_view.count % 3 != 0)
            {
                AddError(out_primitive, "index_topology", "Indexed triangle list requires index count divisible by 3.");
                return;
            }

            out_primitive.vertex_count = static_cast<unsigned>(position_view.count);
            out_primitive.index_count = static_cast<unsigned>(index_view.count);
            out_primitive.geometry.world_positions.resize(position_view.count);
            out_primitive.geometry.world_normals.resize(position_view.count, glm::fvec3(0.0f, 0.0f, 0.0f));
            if (use_uv0_accessor)
            {
                out_primitive.geometry.uv0_vertices.resize(position_view.count);
            }
            out_primitive.geometry.uv1_vertices.resize(uv1_view.count);
            out_primitive.geometry.triangle_indices.resize(index_view.count);
            std::vector<std::uint8_t> normal_valid_mask(position_view.count, 0u);
            std::vector<glm::fvec3> generated_normal_accum(position_view.count, glm::fvec3(0.0f, 0.0f, 0.0f));

            bool uv_bounds_initialized = false;
            glm::fvec2 uv_min{0.0f, 0.0f};
            glm::fvec2 uv_max{0.0f, 0.0f};
            for (std::size_t vertex_index = 0; vertex_index < uv1_view.count; ++vertex_index)
            {
                glm::fvec2 uv{};
                glm::fvec3 position{};
                if (!ReadFloat2(uv1_view, vertex_index, uv))
                {
                    AddError(out_primitive, "uv1_read", "Failed to read TEXCOORD_1 vertex data.");
                    return;
                }

                if (!ReadFloat3(position_view, vertex_index, position))
                {
                    AddError(out_primitive, "position_read", "Failed to read POSITION vertex data.");
                    return;
                }

                out_primitive.geometry.world_positions[vertex_index] = TransformPoint(world_transform, position);
                out_primitive.geometry.uv1_vertices[vertex_index] = uv;
                if (use_uv0_accessor)
                {
                    glm::fvec2 uv0{};
                    if (!ReadFloat2(uv0_view, vertex_index, uv0))
                    {
                        AddWarning(out_primitive,
                                   "uv0_read",
                                   "Failed to read TEXCOORD_0 vertex data. Ignoring TEXCOORD_0 for baker shading textures.");
                        use_uv0_accessor = false;
                        out_primitive.geometry.uv0_vertices.clear();
                    }
                    else
                    {
                        out_primitive.geometry.uv0_vertices[vertex_index] = uv0;
                    }
                }
                if (use_normal_accessor)
                {
                    glm::fvec3 imported_normal{};
                    if (ReadFloat3(normal_view, vertex_index, imported_normal))
                    {
                        const glm::fvec3 world_normal = TransformNormal(world_transform, imported_normal);
                        if (glm::dot(world_normal, world_normal) > 1e-8f)
                        {
                            out_primitive.geometry.world_normals[vertex_index] = world_normal;
                            normal_valid_mask[vertex_index] = 1u;
                        }
                    }
                }

                if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
                {
                    ++out_primitive.uv1_non_finite_vertex_count;
                    continue;
                }

                if (!uv_bounds_initialized)
                {
                    uv_min = uv;
                    uv_max = uv;
                    uv_bounds_initialized = true;
                }
                else
                {
                    uv_min = glm::min(uv_min, uv);
                    uv_max = glm::max(uv_max, uv);
                }

                if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
                {
                    ++out_primitive.uv1_out_of_range_vertex_count;
                }
            }

            out_primitive.uv1_min = uv_bounds_initialized ? uv_min : glm::fvec2(0.0f, 0.0f);
            out_primitive.uv1_max = uv_bounds_initialized ? uv_max : glm::fvec2(0.0f, 0.0f);

            const float uv_area_epsilon = 1e-10f;
            const float position_area_epsilon = 1e-12f;
            for (std::size_t triangle_index = 0; triangle_index < index_view.count; triangle_index += 3)
            {
                unsigned triangle_indices[3]{};
                if (!ReadUnsignedIndex(index_view, triangle_index, triangle_indices[0]) ||
                    !ReadUnsignedIndex(index_view, triangle_index + 1, triangle_indices[1]) ||
                    !ReadUnsignedIndex(index_view, triangle_index + 2, triangle_indices[2]))
                {
                    AddError(out_primitive, "index_read", "Failed to read primitive index data.");
                    return;
                }

                out_primitive.geometry.triangle_indices[triangle_index] = triangle_indices[0];
                out_primitive.geometry.triangle_indices[triangle_index + 1] = triangle_indices[1];
                out_primitive.geometry.triangle_indices[triangle_index + 2] = triangle_indices[2];

                if (triangle_indices[0] >= position_view.count ||
                    triangle_indices[1] >= position_view.count ||
                    triangle_indices[2] >= position_view.count)
                {
                    AddError(out_primitive, "index_range", "Primitive index references a vertex outside accessor range.");
                    return;
                }

                const glm::fvec2 triangle_uv[3] = {
                    out_primitive.geometry.uv1_vertices[triangle_indices[0]],
                    out_primitive.geometry.uv1_vertices[triangle_indices[1]],
                    out_primitive.geometry.uv1_vertices[triangle_indices[2]],
                };
                const glm::fvec3 triangle_positions[3] = {
                    out_primitive.geometry.world_positions[triangle_indices[0]],
                    out_primitive.geometry.world_positions[triangle_indices[1]],
                    out_primitive.geometry.world_positions[triangle_indices[2]],
                };

                const glm::fvec2 uv_edge_01 = triangle_uv[1] - triangle_uv[0];
                const glm::fvec2 uv_edge_02 = triangle_uv[2] - triangle_uv[0];
                const float uv_area = std::abs(uv_edge_01.x * uv_edge_02.y - uv_edge_01.y * uv_edge_02.x);
                if (uv_area <= uv_area_epsilon)
                {
                    ++out_primitive.degenerate_uv_triangle_count;
                }

                const glm::fvec3 position_edge_01 = triangle_positions[1] - triangle_positions[0];
                const glm::fvec3 position_edge_02 = triangle_positions[2] - triangle_positions[0];
                const glm::fvec3 normal_cross = glm::cross(position_edge_01, position_edge_02);
                const float position_area_sq = glm::dot(normal_cross, normal_cross);
                if (position_area_sq <= position_area_epsilon)
                {
                    ++out_primitive.degenerate_position_triangle_count;
                }
                else
                {
                    generated_normal_accum[triangle_indices[0]] += normal_cross;
                    generated_normal_accum[triangle_indices[1]] += normal_cross;
                    generated_normal_accum[triangle_indices[2]] += normal_cross;
                }
            }

            for (std::size_t vertex_index = 0; vertex_index < out_primitive.geometry.world_normals.size(); ++vertex_index)
            {
                if (normal_valid_mask[vertex_index] != 0u)
                {
                    continue;
                }

                const glm::fvec3 generated_normal = generated_normal_accum[vertex_index];
                const float generated_length = glm::length(generated_normal);
                out_primitive.geometry.world_normals[vertex_index] =
                    generated_length > 1e-8f
                        ? (generated_normal / generated_length)
                        : glm::fvec3(0.0f, 1.0f, 0.0f);
            }

            if (out_primitive.uv1_non_finite_vertex_count > 0)
            {
                AddError(out_primitive, "uv1_non_finite", "TEXCOORD_1 contains non-finite vertex values.");
            }

            if (out_primitive.uv1_out_of_range_vertex_count > 0)
            {
                AddError(out_primitive, "uv1_out_of_range", "TEXCOORD_1 contains vertices outside the [0, 1] range.");
            }

            if (out_primitive.degenerate_uv_triangle_count > 0)
            {
                AddError(out_primitive, "uv1_degenerate_triangles", "TEXCOORD_1 contains triangles with zero or near-zero UV area.");
            }

            if (out_primitive.degenerate_position_triangle_count > 0)
            {
                AddWarning(out_primitive, "position_degenerate_triangles", "Primitive contains triangles with zero or near-zero geometric area.");
            }
        }

        void ValidatePrimitive(const glTFLoader& loader,
                               const glTF_Primitive& primitive,
                               const glm::fmat4x4& world_transform,
                               BakePrimitiveImportInfo& out_primitive)
        {
            out_primitive.has_texcoord0 =
                primitive.attributes.contains(glTF_Attribute_TEXCOORD_0::attribute_type_id);
            out_primitive.has_texcoord1 =
                primitive.attributes.contains(glTF_Attribute_TEXCOORD_1::attribute_type_id);

            if (primitive.mode != glTF_Primitive::ETriangles)
            {
                AddError(out_primitive, "primitive_mode", "Only triangle-list primitives are supported in the baker.");
            }

            if (!primitive.attributes.contains(glTF_Attribute_POSITION::attribute_type_id))
            {
                AddError(out_primitive, "missing_position", "Primitive is missing POSITION.");
            }

            if (!primitive.indices.IsValid())
            {
                AddError(out_primitive, "missing_indices", "Primitive is missing indices.");
            }

            if (!out_primitive.has_texcoord1)
            {
                AddError(out_primitive, "missing_uv1", "Primitive is missing TEXCOORD_1.");
            }

            PopulatePrimitiveMaterial(loader, out_primitive, out_primitive.material);
            ValidatePrimitiveGeometry(loader, primitive, world_transform, out_primitive);
            if (out_primitive.material.has_base_color_texture)
            {
                const bool has_required_texcoord =
                    out_primitive.material.base_color_texture_texcoord == 0u
                        ? out_primitive.geometry.uv0_vertices.size() == out_primitive.geometry.world_positions.size()
                        : out_primitive.geometry.uv1_vertices.size() == out_primitive.geometry.world_positions.size();
                if (!has_required_texcoord)
                {
                    AddWarning(out_primitive,
                               out_primitive.material.base_color_texture_texcoord == 0u
                                   ? "base_color_texture_missing_uv0"
                                   : "base_color_texture_missing_uv1",
                               out_primitive.material.base_color_texture_texcoord == 0u
                                   ? "baseColorTexture references TEXCOORD_0, but a valid TEXCOORD_0 stream was not imported. Falling back to factor-only shading."
                                   : "baseColorTexture references TEXCOORD_1, but a valid TEXCOORD_1 stream was not imported. Falling back to factor-only shading.");
                    out_primitive.material.has_base_color_texture = false;
                    out_primitive.material.base_color_texture_uri.clear();
                }
            }
            if (out_primitive.material.has_emissive_texture)
            {
                const bool has_required_texcoord =
                    out_primitive.material.emissive_texture_texcoord == 0u
                        ? out_primitive.geometry.uv0_vertices.size() == out_primitive.geometry.world_positions.size()
                        : out_primitive.geometry.uv1_vertices.size() == out_primitive.geometry.world_positions.size();
                if (!has_required_texcoord)
                {
                    AddWarning(out_primitive,
                               out_primitive.material.emissive_texture_texcoord == 0u
                                   ? "emissive_texture_missing_uv0"
                                   : "emissive_texture_missing_uv1",
                               out_primitive.material.emissive_texture_texcoord == 0u
                                   ? "emissiveTexture references TEXCOORD_0, but a valid TEXCOORD_0 stream was not imported. Falling back to factor-only shading."
                                   : "emissiveTexture references TEXCOORD_1, but a valid TEXCOORD_1 stream was not imported. Falling back to factor-only shading.");
                    out_primitive.material.has_emissive_texture = false;
                    out_primitive.material.emissive_texture_uri.clear();
                }
            }
            out_primitive.can_emit_lightmap_binding = out_primitive.errors.empty();
        }

        void CollectNodePrimitiveInstances(const glTFLoader& loader,
                                           const glTFHandle& node_handle,
                                           const glm::fmat4x4& parent_world_transform,
                                           BakeSceneImportResult& out_result)
        {
            const unsigned node_index = static_cast<unsigned>(loader.ResolveIndex(node_handle));
            const auto& nodes = loader.GetNodes();
            const auto& meshes = loader.GetMeshes();
            if (node_index >= nodes.size())
            {
                AddError(out_result, "node_index", "Scene node index is out of range during import traversal.");
                return;
            }

            const glTF_Element_Node& node = *nodes[node_index];
            const glm::fmat4x4 world_transform = parent_world_transform * node.transform.GetMatrix();
            for (const glTFHandle& mesh_handle : node.meshes)
            {
                const unsigned mesh_index = static_cast<unsigned>(loader.ResolveIndex(mesh_handle));
                if (mesh_index >= meshes.size())
                {
                    AddError(out_result, "mesh_index", "Node references a mesh outside mesh array range.");
                    continue;
                }

                const glTF_Element_Mesh& mesh = *meshes[mesh_index];
                for (unsigned primitive_index = 0; primitive_index < mesh.primitives.size(); ++primitive_index)
                {
                    const glTF_Primitive& primitive = mesh.primitives[primitive_index];
                    BakePrimitiveImportInfo primitive_info{};
                    primitive_info.stable_node_key = node_index;
                    primitive_info.primitive_hash = primitive.Hash();
                    primitive_info.mesh_index = mesh_index;
                    primitive_info.primitive_index = primitive_index;
                    primitive_info.material_index = primitive.material.IsValid()
                        ? static_cast<unsigned>(loader.ResolveIndex(primitive.material))
                        : kInvalidUnsigned;
                    primitive_info.node_name = ResolveNodeName(node, node_index);
                    primitive_info.mesh_name = ResolveMeshName(mesh, mesh_index);

                    ValidatePrimitive(loader, primitive, world_transform, primitive_info);
                    if (primitive_info.can_emit_lightmap_binding)
                    {
                        ++out_result.valid_lightmap_primitive_count;
                    }

                    ++out_result.instance_primitive_count;
                    out_result.primitive_instances.push_back(std::move(primitive_info));
                }
            }

            for (const glTFHandle& child_handle : node.children)
            {
                CollectNodePrimitiveInstances(loader, child_handle, world_transform, out_result);
            }
        }
    }

    bool BakeSceneImportResult::HasErrors() const
    {
        if (!errors.empty())
        {
            return true;
        }

        for (const BakePrimitiveImportInfo& primitive : primitive_instances)
        {
            if (!primitive.errors.empty())
            {
                return true;
            }
        }

        return false;
    }

    bool BakeSceneImportResult::HasValidationErrors() const
    {
        return HasErrors();
    }

    bool BakeSceneImporter::ImportScene(const BakeSceneImportRequest& request,
                                        BakeSceneImportResult& out_result,
                                        std::wstring& out_error) const
    {
        out_result = BakeSceneImportResult{};
        out_result.scene_path = request.scene_path;

        glTFLoader loader{};
        if (!loader.LoadFile(request.scene_path.string()))
        {
            AddError(out_result, "load_file", "Failed to load glTF scene file.");
            out_error = L"Failed to load scene file: " + request.scene_path.native();
            return false;
        }

        out_result.load_succeeded = true;
        out_result.node_count = static_cast<unsigned>(loader.GetNodes().size());
        out_result.mesh_count = static_cast<unsigned>(loader.GetMeshes().size());

        const glTF_Element_Scene& default_scene = loader.GetDefaultScene();
        if (default_scene.root_nodes.empty())
        {
            AddError(out_result, "empty_scene", "Default scene does not contain any root nodes.");
        }

        for (const glTFHandle& root_node_handle : default_scene.root_nodes)
        {
            CollectNodePrimitiveInstances(loader, root_node_handle, glm::fmat4x4(1.0f), out_result);
        }

        if (out_result.instance_primitive_count == 0)
        {
            AddError(out_result, "empty_primitives", "Scene does not contain any mesh primitives.");
        }

        if (out_result.valid_lightmap_primitive_count == 0 && out_result.load_succeeded)
        {
            AddWarning(out_result,
                       "no_valid_lightmap_primitives",
                       "No primitive passed the current TEXCOORD_1 validation gate.");
        }

        return true;
    }
}
