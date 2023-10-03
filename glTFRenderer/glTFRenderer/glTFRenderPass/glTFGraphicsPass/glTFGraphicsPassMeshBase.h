#pragma once

#include "glTFGraphicsPassBase.h"
#include "glTFScene/glTFScenePrimitive.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRenderPass/glTFRenderPassMeshResource.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMesh.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

// Drawing all meshes within mesh pass
class glTFGraphicsPassMeshBase : public glTFGraphicsPassBase, public glTFRenderInterfaceSceneView, public glTFRenderInterfaceSceneMesh
{
protected:
    enum glTFRenderPassMeshBaseRootParameterEnum
    {
        MeshBasePass_RootParameter_SceneView_CBV = 0,
        MeshBasePass_RootParameter_SceneMesh_CBV = 1,
        MeshBasePass_RootParameter_LastIndex,
    };
    
    enum glTFRenderPassMeshBaseRegisterIndex
    {
        // Start with b0
        MeshBasePass_SceneView_CBV_Register = 0,
        
        // Start with b1
        MeshBasePass_SceneMesh_CBV_Register = 1,
    };
    
public:
    virtual const char* PassName() override {return "MeshPass"; }
    bool InitPass(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool RenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;

    bool ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout);
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID);
    virtual bool EndDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID);

    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;

    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertex_input_layouts;

};
