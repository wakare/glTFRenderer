#include "BakeSceneImporter.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
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

        void ValidatePrimitiveGeometry(const glTFLoader& loader,
                                       const glTF_Primitive& primitive,
                                       BakePrimitiveImportInfo& out_primitive)
        {
            const auto it_position = primitive.attributes.find(glTF_Attribute_POSITION::attribute_type_id);
            const auto it_uv1 = primitive.attributes.find(glTF_Attribute_TEXCOORD_1::attribute_type_id);
            if (it_position == primitive.attributes.end() || it_uv1 == primitive.attributes.end() || !primitive.indices.IsValid())
            {
                return;
            }

            AccessorDataView position_view{};
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

            bool uv_bounds_initialized = false;
            glm::fvec2 uv_min{0.0f, 0.0f};
            glm::fvec2 uv_max{0.0f, 0.0f};
            for (std::size_t vertex_index = 0; vertex_index < uv1_view.count; ++vertex_index)
            {
                glm::fvec2 uv{};
                if (!ReadFloat2(uv1_view, vertex_index, uv))
                {
                    AddError(out_primitive, "uv1_read", "Failed to read TEXCOORD_1 vertex data.");
                    return;
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

                if (triangle_indices[0] >= position_view.count ||
                    triangle_indices[1] >= position_view.count ||
                    triangle_indices[2] >= position_view.count)
                {
                    AddError(out_primitive, "index_range", "Primitive index references a vertex outside accessor range.");
                    return;
                }

                glm::fvec2 triangle_uv[3]{};
                glm::fvec3 triangle_positions[3]{};
                if (!ReadFloat2(uv1_view, triangle_indices[0], triangle_uv[0]) ||
                    !ReadFloat2(uv1_view, triangle_indices[1], triangle_uv[1]) ||
                    !ReadFloat2(uv1_view, triangle_indices[2], triangle_uv[2]) ||
                    !ReadFloat3(position_view, triangle_indices[0], triangle_positions[0]) ||
                    !ReadFloat3(position_view, triangle_indices[1], triangle_positions[1]) ||
                    !ReadFloat3(position_view, triangle_indices[2], triangle_positions[2]))
                {
                    AddError(out_primitive, "triangle_read", "Failed to read triangle attribute data.");
                    return;
                }

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

            ValidatePrimitiveGeometry(loader, primitive, out_primitive);
            out_primitive.can_emit_lightmap_binding = out_primitive.errors.empty();
        }

        void CollectNodePrimitiveInstances(const glTFLoader& loader,
                                           const glTFHandle& node_handle,
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

                    ValidatePrimitive(loader, primitive, primitive_info);
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
                CollectNodePrimitiveInstances(loader, child_handle, out_result);
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
            CollectNodePrimitiveInstances(loader, root_node_handle, out_result);
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
