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
#include "../glTFScene/glTFSceneView.h"

struct ConstantBufferPerMeshDraw
{
    glm::mat4 worldMat {glm::mat4(1.0f)};
    glm::mat4 viewProjection {glm::mat4{1.0f}};
};

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
    
    std::map<glTFUniqueID, MeshGPUResource> m_meshes;

    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertexInputLayouts;

    std::shared_ptr<IRHIRenderTarget> m_basePassColorRenderTarget;
};
