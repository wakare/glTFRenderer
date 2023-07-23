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
                size_t vertexBufferSize = 0;

                std::vector<char*> vertexDataInGLTFBuffers;

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
				    primitive, vertexBufferSize, vertexLayout, vertexDataInGLTFBuffers);

                // NORMAL attribute
                _process_vertex_attribute(loader, glTF_Attribute_NORMAL::attribute_type_id, VertexLayoutType::NORMAL,
                    primitive, vertexBufferSize, vertexLayout, vertexDataInGLTFBuffers);

                // TEXCOORD attribute
                _process_vertex_attribute(loader, glTF_Attribute_TEXCOORD_0::attribute_type_id, VertexLayoutType::TEXCOORD_0,
                    primitive, vertexBufferSize, vertexLayout, vertexDataInGLTFBuffers);

                std::shared_ptr<VertexBufferData> vertexBufferData = std::make_shared<VertexBufferData>();
                vertexBufferData->data.reset(new char[vertexBufferSize]);
                vertexBufferData->byteSize = vertexBufferSize;
                vertexBufferData->vertexCount = vertexBufferSize /vertexLayout.GetVertexStrideInBytes();
                char* vertexDataStart = vertexBufferData->data.get();
                for (size_t v = 0; v < vertexBufferData->vertexCount; ++v)
                {
                    // Reformat vertex buffer data
                    for (size_t i = 0; i < vertexLayout.elements.size(); ++i)
                    {
                        if (vertexLayout.elements[i].type == VertexLayoutType::POSITION)
                        {
                            // Position attribute component type should be float
                            GLTF_CHECK(vertexLayout.elements[i].byte_size == 3 * sizeof(float));
                            auto position = reinterpret_cast<float*>(vertexDataInGLTFBuffers[i]);
                            mesh_AABB.extend({position[0], position[1], position[2]});
                        }
                        
                        memcpy(vertexDataStart, vertexDataInGLTFBuffers[i], vertexLayout.elements[i].byte_size);
                        vertexDataInGLTFBuffers[i] += vertexLayout.elements[i].byte_size;
                        vertexDataStart += vertexLayout.elements[i].byte_size;
                    }
                }
            
                const auto& indexAccessor = *loader.m_accessors[loader.ResolveIndex(primitive.indices)];
                const auto& indexBufferView = *loader.m_bufferViews[loader.ResolveIndex(indexAccessor.buffer_view)];

                const size_t indexBufferSize = indexAccessor.GetElementByteSize() * indexAccessor.count;
                
                std::shared_ptr<IndexBufferData> indexBufferData = std::make_shared<IndexBufferData>();
                indexBufferData->data.reset(new char[indexBufferSize]);
                glTFHandle tempIndexBufferViewHandle = indexBufferView.buffer;
                tempIndexBufferViewHandle.node_index = loader.ResolveIndex(indexBufferView.buffer);
                auto findIt = loader.m_buffer_data.find(tempIndexBufferViewHandle);
                GLTF_CHECK(findIt != loader.m_buffer_data.end());
                const char* bufferStart = findIt->second.get() + indexBufferView.byte_offset + indexAccessor.byte_offset;
                memcpy(indexBufferData->data.get(), bufferStart, indexBufferSize);
                indexBufferData->byteSize = indexBufferSize;
                indexBufferData->indexCount = indexAccessor.count;
                indexBufferData->elementType = indexAccessor.component_type ==
                    glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedShort ?
                    IndexBufferElementType::UNSIGNED_SHORT : IndexBufferElementType::UNSIGNED_INT;   

				std::unique_ptr<glTFSceneTriangleMesh> triangle_mesh =
				    std::make_unique<glTFSceneTriangleMesh>(sceneNode.m_finalTransform, vertexLayout, vertexBufferData, indexBufferData);
				std::shared_ptr<glTFMaterialPBR> pbr_material = std::make_shared<glTFMaterialPBR>();
				triangle_mesh->SetMaterial(pbr_material);

                // Only set base color texture now, support other feature in the future
                const auto& source_material =
                    *loader.m_materials[loader.ResolveIndex(primitive.material)]; 
				const auto& base_color_texture =
				    *loader.m_textures[loader.ResolveIndex(source_material.pbr.base_color_texture.index)];
                const auto& texture_image =
                    *loader.m_images[loader.ResolveIndex(base_color_texture.source)];
                GLTF_CHECK(!texture_image.uri.empty());

                pbr_material->AddOrUpdateMaterialParameter(glTFMaterialParameterUsage::BASECOLOR,
                    std::make_shared<glTFMaterialParameterTexture>(loader.GetSceneFileDirectory() + texture_image.uri, glTFMaterialParameterUsage::BASECOLOR));
                
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