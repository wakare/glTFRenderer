#include "glTFSceneGraph.h"

#include "glTFAABB.h"
#include "glTFMeshRawData.h"
#include "glTFSceneTriangleMesh.h"
#include "../glTFMaterial/glTFMaterialPBR.h"
#include "glTFMaterial/glTFMaterialParameterFactor.h"

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

void glTFSceneNode::ResetDirty() const
{
    for (const auto& object : m_objects)
    {
        if (object->IsDirty())
        {
            object->ResetDirty();
        }
    }

    for (const auto& child : m_children)
    {
        child->ResetDirty();
    }
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
            
            for (const auto& primitive : mesh.primitives)
            {
                if (mesh_datas.find(primitive.Hash()) == mesh_datas.end())
                {
                    // Create mesh raw data
                    mesh_datas[primitive.Hash()] = std::make_shared<glTFMeshRawData>(loader, primitive);
                }
                
				std::unique_ptr<glTFSceneTriangleMesh> triangle_mesh =
				    std::make_unique<glTFSceneTriangleMesh>(sceneNode.m_finalTransform, mesh_datas[primitive.Hash()]);

                const glTFHandle material_id = primitive.material;
                if (mesh_materials.find(material_id) == mesh_materials.end())
                {
                    std::shared_ptr<glTFMaterialPBR> pbr_material = std::make_shared<glTFMaterialPBR>();
                    triangle_mesh->SetMaterial(pbr_material);

                    // Only set base color texture now, support other feature in the future
                    const auto& source_material =
                        *loader.m_materials[loader.ResolveIndex(material_id)];

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
                        pbr_material->AddOrUpdateMaterialParameter(glTFMaterialParameterUsage::BASECOLOR,
                            std::make_shared<glTFMaterialParameterFactor<glm::vec4>>(glTFMaterialParameterUsage::BASECOLOR, source_material.pbr.base_color_factor));
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

                    // Metallic roughness texture setting
                    const auto metallic_roughness_texture_handle = source_material.pbr.metallic_roughness_texture.index; 
                    if (metallic_roughness_texture_handle.IsValid())
                    {
                        const auto& metallic_roughness_texture = *loader.m_textures[loader.ResolveIndex(metallic_roughness_texture_handle)];
                        const auto& texture_image = *loader.m_images[loader.ResolveIndex(metallic_roughness_texture.source)];
                        GLTF_CHECK(!texture_image.uri.empty());

                        pbr_material->AddOrUpdateMaterialParameter(glTFMaterialParameterUsage::METALLIC_ROUGHNESS,
                            std::make_shared<glTFMaterialParameterTexture>(loader.GetSceneFileDirectory() + texture_image.uri, glTFMaterialParameterUsage::METALLIC_ROUGHNESS));
                    }

                    mesh_materials[material_id] = pbr_material;
                }
                else
                {
                    triangle_mesh->SetMaterial(mesh_materials[material_id]);
                }

                auto AABB = triangle_mesh->GetAABB();
                sceneNode.m_objects.push_back(std::move(triangle_mesh));
                sceneNode.m_objects.back()->SetAABB(AABB);
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