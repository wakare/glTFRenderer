#pragma once
#include <map>

#include "glTFGraphicsPassBase.h"
#include "glTFRenderPassBase.h"
#include "glTFRenderPassInterfaceSceneMesh.h"
#include "glTFRenderPassInterfaceSceneView.h"
#include "../glTFRHI/RHIInterface/IRHIGPUBuffer.h"
#include "../glTFScene/glTFScenePrimitive.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIVertexBufferView.h"
#include "../glTFRHI/RHIInterface/IRHIIndexBufferView.h"

// Vertex and index gpu buffer data
struct MeshGPUResource
{
    std::shared_ptr<IRHIGPUBuffer> mesh_vertex_buffer;
    std::shared_ptr<IRHIGPUBuffer> mesh_index_buffer;
    std::shared_ptr<IRHIVertexBufferView> mesh_vertex_buffer_view;
    std::shared_ptr<IRHIIndexBufferView> mesh_index_buffer_view;

    size_t mesh_vertex_count{0};
    size_t mesh_index_count{0};
    
    glm::mat4 meshTransformMatrix{1.0f};
    glTFUniqueID material_id {glTFUniqueIDInvalid};
    bool using_normal_mapping {false};
};

// Drawing all meshes within mesh pass
class glTFRenderPassMeshBase : public glTFGraphicsPassBase, public glTFRenderPassInterfaceSceneView, public glTFRenderPassInterfaceSceneMesh
{
protected:
    enum glTFRenderPassMeshBaseRootParameterEnum
    {
        MeshBasePass_RootParameter_SceneView_CBV = 0,
        MeshBasePass_RootParameter_SceneMesh_CBV = 1,
        MeshBasePass_RootParameter_SceneMesh_SRV = 2,
        MeshBasePass_RootParameter_LastIndex,
    };
    enum glTFRenderPassMeshBaseRegisterIndex
    {
        // Start with b0
        MeshBasePass_SceneView_CBV_Register = 0,
        
        // Start with b1
        MeshBasePass_SceneMesh_CBV_Register = 1,
        
        // Start with t0
        MeshBasePass_SceneMesh_SRV_Register = 0,
    };
    
public:
    glTFRenderPassMeshBase();
    
    virtual const char* PassName() override {return "MeshPass"; }
    bool InitPass(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool RenderPass(glTFRenderResourceManager& resource_manager) override;

    bool AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive& primitive);
    bool RemovePrimitiveFromMeshPass(glTFUniqueID mesh_id_to_remove);

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;

    bool ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout);
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID);
    virtual bool EndDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID);

    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;
    
    std::map<glTFUniqueID, MeshGPUResource> m_meshes;

    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertex_input_layouts;

};
