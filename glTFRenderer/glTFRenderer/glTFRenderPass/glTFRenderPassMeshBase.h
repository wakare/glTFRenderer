#pragma once
#include <map>

#include "glTFRenderPassBase.h"
#include "../glTFRHI/RHIInterface/IRHIGPUBuffer.h"
#include "../glTFScene/glTFScenePrimitive.h"
#include "../glTFRHI/RHIInterface/IRHIRootSignature.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIVertexBufferView.h"
#include "../glTFRHI/RHIInterface/IRHIIndexBufferView.h"
#include "../glTFRHI/RHIInterface/IRHIDescriptorHeap.h"
#include "../glTFScene/glTFSceneView.h"

struct ConstantBufferPerMesh
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
class glTFRenderPassMeshBase : public glTFRenderPassBase
{
public:
    glTFRenderPassMeshBase();
    
    virtual const char* PassName() override {return "MeshPass"; }
    bool InitPass(glTFRenderResourceManager& resourceManager) override;
    bool RenderPass(glTFRenderResourceManager& resourceManager) override;

    bool AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resourceManager, const glTFScenePrimitive& primitive);
    bool RemovePrimitiveFromMeshPass(glTFUniqueID meshIDToRemove);

    void UpdateViewParameters(const glTFSceneView& view);
    
protected:
    virtual size_t GetMainDescriptorHeapSize() = 0;
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() = 0;
    
    ConstantBufferPerMesh m_constantBufferPerObject;
    
    std::shared_ptr<IRHIRenderTarget> m_depthBuffer;
    std::shared_ptr<IRHIRootSignature> m_rootSignature;
    std::shared_ptr<IRHIPipelineStateObject> m_pipelineStateObject;

    std::shared_ptr<IRHIGPUBuffer> m_perMeshConstantBuffer;
    RHIGPUDescriptorHandle m_perMeshCBHandle;
    
    // CBV_SRV_UAV Heaps, can only bind one in render pass
    std::shared_ptr<IRHIDescriptorHeap> m_mainDescriptorHeap;
    
    std::map<glTFUniqueID, MeshGPUResource> m_meshes;

    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertexInputLayouts;
};
