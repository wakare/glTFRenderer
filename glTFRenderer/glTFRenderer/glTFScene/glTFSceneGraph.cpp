#include "glTFSceneGraph.h"

#include "glTFAABB.h"
#include "glTFSceneTriangleMesh.h"
#include "../glTFMaterial/glTFMaterialPBR.h"

bool glTFSceneNode::IsDirty() const
{
    for (const auto& object : m_objects)
    {
        if (object->IsDirty())
        {
            return true;
        }
    }

    for (const auto& child : m_children)
    {
        if (child->IsDirty())
        {
            return true;
        }
    }

    return false;
}

glTFSceneGraph::glTFSceneGraph()
    : m_root(std::make_unique<glTFSceneNode>())
{
    m_root->m_finalTransform = m_root->m_transform.GetTransformMatrix();
}

void glTFSceneGraph::AddSceneNode(std::unique_ptr<glTFSceneNode>&& node)
{
    assert(node.get());
    m_root->m_children.push_back(std::move(node));
}

void glTFSceneGraph::Tick(size_t deltaTimeMs)
{
    TraverseNodesInner([](glTFSceneNode& node)
    {
        for (const auto& sceneObject : node.m_objects)
        {
            if (sceneObject->CanTick())
            {
                sceneObject->Tick();
            }    
        }
        
        return true;
    });
}

bool TraverseNodeImpl(const std::function<bool(glTFSceneNode&)>& visitor, glTFSceneNode& nodeToVisit)
{
    bool needVisitorNext = visitor(nodeToVisit);
    if (needVisitorNext && !nodeToVisit.m_children.empty())
    {
        for (const auto& child : nodeToVisit.m_children)
        {
            needVisitorNext = TraverseNodeImpl(visitor, *child);
            if (!needVisitorNext)
            {
                return false;
            }
        }
    }

    return needVisitorNext;
}

void glTFSceneGraph::TraverseNodes(const std::function<bool(const glTFSceneNode&)>& visitor) const
{
    TraverseNodeImpl(visitor, *m_root);
}

std::vector<glTFCamera*> glTFSceneGraph::GetSceneCameras() const
{
    std::vector<glTFCamera*> cameras;
    TraverseNodes([&cameras](const glTFSceneNode& node)
    {
        for (const auto& sceneObject : node.m_objects)
        {
            if (auto* camera = dynamic_cast<glTFCamera*>(sceneObject.get()) )
            {
                cameras.push_back(camera);
            }    
        }
            
        return true;
    });

    return cameras;
}

const glTFSceneNode& glTFSceneGraph::GetRootNode() const
{
    return *m_root;
}

bool glTFSceneGraph::Init(const glTFLoader& loader)
{
    const auto& sceneNode = loader.m_scenes[loader.m_default_scene];
    for (const auto& rootNode : sceneNode->root_nodes)
    {
        std::unique_ptr<glTFSceneNode> sceneRootNodes = std::make_unique<glTFSceneNode>();
        RecursiveInitSceneNodeFromGLTFLoader(loader, rootNode, *m_root, *sceneRootNodes);
        
        AddSceneNode(std::move(sceneRootNodes));
    }
    
    return true;
}

void glTFSceneGraph::TraverseNodesInner(const std::function<bool(glTFSceneNode&)>& visitor) const
{
    TraverseNodeImpl(visitor, *m_root);
}

void glTFSceneGraph::RecursiveInitSceneNodeFromGLTFLoader(const glTFLoader& loader, const glTFHandle& handle,
    const glTFSceneNode& parentNode, glTFSceneNode& sceneNode)
{
    const auto& node = loader.m_nodes[loader.ResolveIndex(handle)];
    sceneNode.m_transform = node->transform.GetMatrix();
    sceneNode.m_finalTransform = parentNode.m_finalTransform.GetTransformMatrix() * sceneNode.m_transform.GetTransformMatrix(); 

    for (const auto& mesh_handle : node->meshes)
    {
        if (mesh_handle.IsValid())
        {
            const auto& mesh = *loader.m_meshes[loader.ResolveIndex(mesh_handle)];
            glTF_AABB::AABB mesh_AABB;
            
            for (const auto& primitive : mesh.primitives)
            {
                VertexLayoutDeclaration vertexLayout;
                size_t vertex_buffer_size = 0;

                std::vector<char*> vertex_data_in_buffers;

				static auto _process_vertex_attribute = [](const glTFLoader& source_loader, glTFAttributeId attribute_ID, VertexLayoutType attribute_type,
				    const glTF_Primitive& source_primitive, size_t& out_vertex_buffer_size, VertexLayoutDeclaration& out_vertex_layout,
				    std::vector<char*>& out_vertex_data_infos)
				{
                    const auto itPosition = source_primitive.attributes.find(attribute_ID);
				    if (itPosition != source_primitive.attributes.end())
				    {
				        const glTFHandle accessorHandle = itPosition->second; 
				        const auto& vertexAccessor = *source_loader.m_accessors[source_loader.ResolveIndex(accessorHandle)];
				        out_vertex_layout.elements.push_back({attribute_type, vertexAccessor.GetElementByteSize()});
				        out_vertex_buffer_size += vertexAccessor.count * vertexAccessor.GetElementByteSize();

				        const auto& vertexBufferView = *source_loader.m_bufferViews[source_loader.ResolveIndex(vertexAccessor.buffer_view)];
				        glTFHandle tempVertexBufferViewHandle = vertexBufferView.buffer;
				        tempVertexBufferViewHandle.node_index = source_loader.ResolveIndex(vertexBufferView.buffer);
                        const auto findIt = source_loader.m_buffer_data.find(tempVertexBufferViewHandle);
				        GLTF_CHECK(findIt != source_loader.m_buffer_data.end());
				        char* position_data = findIt->second.get() + vertexBufferView.byte_offset + vertexAccessor.byte_offset;
				        out_vertex_data_infos.push_back(position_data);
				    }
				};
                
                // POSITION attribute
				_process_vertex_attribute(loader, glTF_Attribute_POSITION::attribute_type_id, VertexLayoutType::POSITION,
				    primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);

                // NORMAL attribute
                _process_vertex_attribute(loader, glTF_Attribute_NORMAL::attribute_type_id, VertexLayoutType::NORMAL,
                    primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);

                // TANGENT attribute
                _process_vertex_attribute(loader, glTF_Attribute_TANGENT::attribute_type_id, VertexLayoutType::TANGENT,
                    primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);
                
                // TEXCOORD attribute
                _process_vertex_attribute(loader, glTF_Attribute_TEXCOORD_0::attribute_type_id, VertexLayoutType::TEXCOORD_0,
                    primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);

                
                
                std::shared_ptr<VertexBufferData> vertex_buffer_data = std::make_shared<VertexBufferData>();
                vertex_buffer_data->data.reset(new char[vertex_buffer_size]);
                vertex_buffer_data->byteSize = vertex_buffer_size;
                vertex_buffer_data->vertex_count = vertex_buffer_size / vertexLayout.GetVertexStrideInBytes();
                vertex_buffer_data->layout = vertexLayout;

                std::shared_ptr<VertexBufferData> position_only_data = std::make_shared<VertexBufferData>();
                const size_t position_only_data_size = vertex_buffer_data->vertex_count * 3 * sizeof(float);
                position_only_data->data.reset(new char[position_only_data_size]);
                position_only_data->byteSize = position_only_data_size;
                position_only_data->vertex_count = vertex_buffer_data->vertex_count;
                position_only_data->layout.elements.push_back({VertexLayoutType::POSITION, 12});
                
                char* vertex_data_start = vertex_buffer_data->data.get();
                char* position_only_data_start = position_only_data->data.get();
                
                for (size_t v = 0; v < vertex_buffer_data->vertex_count; ++v)
                {
                    // Reformat vertex buffer data
                    for (size_t i = 0; i < vertexLayout.elements.size(); ++i)
                    {
                        if (vertexLayout.elements[i].type == VertexLayoutType::POSITION)
                        {
                            // Position attribute component type should be float
                            GLTF_CHECK(vertexLayout.elements[i].byte_size == 3 * sizeof(float));
                            auto position = reinterpret_cast<float*>(vertex_data_in_buffers[i]);
                            mesh_AABB.extend({position[0], position[1], position[2]});

                            memcpy(position_only_data_start, vertex_data_in_buffers[i], vertexLayout.elements[i].byte_size);
                        }
                        
                        memcpy(vertex_data_start, vertex_data_in_buffers[i], vertexLayout.elements[i].byte_size);
                        vertex_data_in_buffers[i] += vertexLayout.elements[i].byte_size;
                        vertex_data_start += vertexLayout.elements[i].byte_size;
                    }
                }
            
                const auto& index_accessor = *loader.m_accessors[loader.ResolveIndex(primitive.indices)];
                const auto& index_buffer_view = *loader.m_bufferViews[loader.ResolveIndex(index_accessor.buffer_view)];

                const size_t index_buffer_size = index_accessor.GetElementByteSize() * index_accessor.count;
                
                std::shared_ptr<IndexBufferData> index_buffer_data = std::make_shared<IndexBufferData>();
                index_buffer_data->data.reset(new char[index_buffer_size]);
                glTFHandle tempIndexBufferViewHandle = index_buffer_view.buffer;
                tempIndexBufferViewHandle.node_index = loader.ResolveIndex(index_buffer_view.buffer);
                auto findIt = loader.m_buffer_data.find(tempIndexBufferViewHandle);
                GLTF_CHECK(findIt != loader.m_buffer_data.end());
                const char* bufferStart = findIt->second.get() + index_buffer_view.byte_offset + index_accessor.byte_offset;
                memcpy(index_buffer_data->data.get(), bufferStart, index_buffer_size);
                index_buffer_data->byteSize = index_buffer_size;
                index_buffer_data->index_count = index_accessor.count;
                index_buffer_data->format = index_accessor.component_type ==
                    glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedShort ?
                    RHIDataFormat::R16_UINT : RHIDataFormat::R32_UINT;   

				std::unique_ptr<glTFSceneTriangleMesh> triangle_mesh =
				    std::make_unique<glTFSceneTriangleMesh>(sceneNode.m_finalTransform, vertex_buffer_data, index_buffer_data, position_only_data);
                
				std::shared_ptr<glTFMaterialPBR> pbr_material = std::make_shared<glTFMaterialPBR>();
				triangle_mesh->SetMaterial(pbr_material);

                // Only set base color texture now, support other feature in the future
                const auto& source_material =
                    *loader.m_materials[loader.ResolveIndex(primitive.material)];

                // Base color texture setting
                const auto base_color_texture_handle = source_material.pbr.base_color_texture.index;
                if (base_color_texture_handle.IsValid())
                {
                    const auto& base_color_texture = *loader.m_textures[loader.ResolveIndex(base_color_texture_handle)];
                    const auto& texture_image = *loader.m_images[loader.ResolveIndex(base_color_texture.source)];
                    GLTF_CHECK(!texture_image.uri.empty());

                    pbr_material->AddOrUpdateMaterialParameter(glTFMaterialParameterUsage::BASECOLOR,
                        std::make_shared<glTFMaterialParameterTexture>(loader.GetSceneFileDirectory() + texture_image.uri, glTFMaterialParameterUsage::BASECOLOR));    
                }
                else
                {
                    // No base color texture? handle base color factor in the future
                    GLTF_CHECK(false);
                }

                // Normal texture setting
                const auto normal_texture_handle = source_material.normal_texture.index; 
                if (normal_texture_handle.IsValid())
                {
                    const auto& normal_texture = *loader.m_textures[loader.ResolveIndex(normal_texture_handle)];
                    const auto& texture_image = *loader.m_images[loader.ResolveIndex(normal_texture.source)];
                    GLTF_CHECK(!texture_image.uri.empty());

                    pbr_material->AddOrUpdateMaterialParameter(glTFMaterialParameterUsage::NORMAL,
                        std::make_shared<glTFMaterialParameterTexture>(loader.GetSceneFileDirectory() + texture_image.uri, glTFMaterialParameterUsage::NORMAL));
                }
                
                sceneNode.m_objects.push_back(std::move(triangle_mesh));
                sceneNode.m_objects.back()->SetAABB(mesh_AABB);
            }
        }
    }
    
    for (const auto& child : node->children)
    {
        std::unique_ptr<glTFSceneNode> childNode = std::make_unique<glTFSceneNode>();
        RecursiveInitSceneNodeFromGLTFLoader(loader, child, sceneNode, *childNode);
        sceneNode.m_children.push_back(std::move(childNode));
    }
}