#include "glTFSceneGraph.h"

#include "glTFAABB.h"
#include "glTFSceneTriangleMesh.h"

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
    m_root->m_transform = glTF_Transform_WithTRS();
    m_root->m_finalTransform = m_root->m_transform.m_matrix;
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
    const auto& sceneNode = loader.m_scenes[loader.default_scene];
    for (const auto& rootNode : sceneNode->root_nodes)
    {
        std::unique_ptr<glTFSceneNode> sceneRootNodes = std::make_unique<glTFSceneNode>();
        RecursiveInitChildrenNodes(loader, rootNode, *m_root, *sceneRootNodes);
        
        AddSceneNode(std::move(sceneRootNodes));
    }
    
    return true;
}

void glTFSceneGraph::TraverseNodesInner(const std::function<bool(glTFSceneNode&)>& visitor) const
{
    TraverseNodeImpl(visitor, *m_root);
}

void glTFSceneGraph::RecursiveInitChildrenNodes(const glTFLoader& loader, const glTFHandle& handle,
    const glTFSceneNode& parentNode, glTFSceneNode& sceneNode)
{
    const auto& node = loader.m_nodes[loader.ResolveIndex(handle)];
    sceneNode.m_transform = node->transform.m_matrix;
    sceneNode.m_finalTransform = parentNode.m_finalTransform.m_matrix * sceneNode.m_transform.m_matrix; 

    for (const auto& mesh_handle : node->meshes)
    {
        if (mesh_handle.IsValid())
        {
            const auto& mesh = *loader.m_meshes[loader.ResolveIndex(mesh_handle)];
            glTF_AABB::AABB meshAABB;
            
            for (const auto& primitive : mesh.primitives)
            {
                VertexLayoutDeclaration vertexLayout;
                size_t vertexBufferSize = 0;

                std::vector<char*> vertexDataInGLTFBuffers;
            
                // POSITION attribute
                auto itPosition = primitive.attributes.find(glTF_Attribute_POSITION::attribute_type_id);
                if (itPosition != primitive.attributes.end())
                {
                    const glTFHandle accessorHandle = itPosition->second; 
                    const auto& vertexAccessor = *loader.m_accessors[loader.ResolveIndex(accessorHandle)];
                    vertexLayout.elements.push_back({VertexLayoutType::POSITION, vertexAccessor.GetElementByteSize()});
                    vertexBufferSize += vertexAccessor.count * vertexAccessor.GetElementByteSize();

                    const auto& vertexBufferView = *loader.m_bufferViews[loader.ResolveIndex(vertexAccessor.buffer_view)];
                    glTFHandle tempVertexBufferViewHandle = vertexBufferView.buffer;
                    tempVertexBufferViewHandle.node_index = loader.ResolveIndex(vertexBufferView.buffer);
                    auto findIt = loader.m_bufferDatas.find(tempVertexBufferViewHandle);
                    GLTF_CHECK(findIt != loader.m_bufferDatas.end());
                    char* position_data = findIt->second.get() + vertexBufferView.byte_offset + vertexAccessor.byte_offset;
                    vertexDataInGLTFBuffers.push_back(position_data);
                }

                // NORMAL attribute
                auto itNormal = primitive.attributes.find(glTF_Attribute_NORMAL::attribute_type_id);
                if (itNormal != primitive.attributes.end())
                {
                    const glTFHandle accessorHandle = itNormal->second; 
                    const auto& vertexAccessor = *loader.m_accessors[loader.ResolveIndex(accessorHandle)];
                    vertexLayout.elements.push_back({VertexLayoutType::NORMAL, vertexAccessor.GetElementByteSize()});
                    vertexBufferSize += vertexAccessor.count * vertexAccessor.GetElementByteSize();

                    const auto& vertexBufferView = *loader.m_bufferViews[loader.ResolveIndex(vertexAccessor.buffer_view)];
                    glTFHandle tempVertexBufferViewHandle = vertexBufferView.buffer;
                    tempVertexBufferViewHandle.node_index = loader.ResolveIndex(vertexBufferView.buffer);
                    auto findIt = loader.m_bufferDatas.find(tempVertexBufferViewHandle);
                    GLTF_CHECK(findIt != loader.m_bufferDatas.end());
                    char* bufferStart = findIt->second.get();
                    vertexDataInGLTFBuffers.push_back(bufferStart + vertexBufferView.byte_offset + vertexAccessor.byte_offset);
                }

                std::shared_ptr<VertexBufferData> vertexBufferData = std::make_shared<VertexBufferData>();
                vertexBufferData->data.reset(new char[vertexBufferSize]);
                vertexBufferData->byteSize = vertexBufferSize;
                vertexBufferData->vertexCount = vertexBufferSize /vertexLayout.GetVertexStride();
                char* vertexDataStart = vertexBufferData->data.get();
                for (size_t v = 0; v < vertexBufferData->vertexCount; ++v)
                {
                    // Reformat vertex buffer data
                    for (size_t i = 0; i < vertexLayout.elements.size(); ++i)
                    {
                        if (vertexLayout.elements[i].type == VertexLayoutType::POSITION)
                        {
                            // Position attribute component type should be float
                            GLTF_CHECK(vertexLayout.elements[i].byteSize == 3 * sizeof(float));
                            auto position = reinterpret_cast<float*>(vertexDataInGLTFBuffers[i]);
                            meshAABB.extend({position[0], position[1], position[2]});
                        }
                        
                        memcpy(vertexDataStart, vertexDataInGLTFBuffers[i], vertexLayout.elements[i].byteSize);
                        vertexDataInGLTFBuffers[i] += vertexLayout.elements[i].byteSize;
                        vertexDataStart += vertexLayout.elements[i].byteSize;
                    }
                }
            
                const auto& indexAccessor = *loader.m_accessors[loader.ResolveIndex(primitive.indices)];
                const auto& indexBufferView = *loader.m_bufferViews[loader.ResolveIndex(indexAccessor.buffer_view)];

                const size_t indexBufferSize = indexAccessor.GetElementByteSize() * indexAccessor.count;
                
                std::shared_ptr<IndexBufferData> indexBufferData = std::make_shared<IndexBufferData>();
                indexBufferData->data.reset(new char[indexBufferSize]);
                glTFHandle tempIndexBufferViewHandle = indexBufferView.buffer;
                tempIndexBufferViewHandle.node_index = loader.ResolveIndex(indexBufferView.buffer);
                auto findIt = loader.m_bufferDatas.find(tempIndexBufferViewHandle);
                GLTF_CHECK(findIt != loader.m_bufferDatas.end());
                const char* bufferStart = findIt->second.get() + indexBufferView.byte_offset + indexAccessor.byte_offset;
                memcpy(indexBufferData->data.get(), bufferStart, indexBufferSize);
                indexBufferData->byteSize = indexBufferSize;
                indexBufferData->indexCount = indexAccessor.count;
                indexBufferData->elementType = indexAccessor.component_type == EUnsignedShort ? IndexBufferElementType::UNSIGNED_SHORT : IndexBufferElementType::UNSIGNED_INT;   
            
                sceneNode.m_objects.push_back(std::make_unique<glTFSceneTriangleMesh>(sceneNode.m_finalTransform, vertexLayout, vertexBufferData, indexBufferData));
                sceneNode.m_objects.back()->SetAABB(meshAABB);
            }
        }
    }
    
    for (const auto& child : node->children)
    {
        std::unique_ptr<glTFSceneNode> childNode = std::make_unique<glTFSceneNode>();
        RecursiveInitChildrenNodes(loader, child, sceneNode, *childNode);
        sceneNode.m_children.push_back(std::move(childNode));
    }
}