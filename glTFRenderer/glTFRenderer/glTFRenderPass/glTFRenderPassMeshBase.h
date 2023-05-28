#pragma once
#include <map>

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
    std::shared_ptr<IRHIGPUBuffer> meshVertexBuffer;
    std::shared_ptr<IRHIGPUBuffer> meshIndexBuffer;
    std::shared_ptr<IRHIVertexBufferView> meshVertexBufferView;
    std::shared_ptr<IRHIIndexBufferView> meshIndexBufferView;

    size_t meshVertexCount{0};
    size_t meshIndexCount{0};
    
    glm::mat4 meshTransformMatrix;
    glTFUniqueID materialID;
};

// Drawing all meshes within mesh pass
class glTFRenderPassMeshBase : public glTFRenderPassBase, public glTFRenderPassInterfaceSceneView, public glTFRenderPassInterfaceSceneMesh
{
protected:
    enum glTFRenderPassMeshBaseRootParameterEnum
    {
        MeshBasePass_RootParameter_SceneView = 0,
        MeshBasePass_RootParameter_SceneMesh = 1,
        MeshBasePass_RootParameter_Num,
    };
public:
    glTFRenderPassMeshBase();
    
    virtual const char* PassName() override {return "MeshPass"; }
    bool InitPass(glTFRenderResourceManager& resourceManager) override;
    bool RenderPass(glTFRenderResourceManager& resourceManager) override;

    bool AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resourceManager, const glTFScenePrimitive& primitive);
    bool RemovePrimitiveFromMeshPass(glTFUniqueID meshIDToRemove);

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID);
    virtual bool EndDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID);
    
    std::map<glTFUniqueID, MeshGPUResource> m_meshes;

    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertexInputLayouts;

    std::shared_ptr<IRHIRenderTarget> m_basePassColorRenderTarget;
    std::shared_ptr<IRHIRenderTarget> m_basePassNormalRenderTarget;
};
