#pragma once
#include "glTFRenderResourceManager.h"
#include "../glTFMaterial/glTFMaterialBase.h"

class IRHIRootSignature;
class IRHIPipelineStateObject;
class IRHIDescriptorHeap;
class IRHICommandList;

class glTFRenderPassBase 
{
public:
    glTFRenderPassBase() = default;
    
    glTFRenderPassBase(const glTFRenderPassBase&) = delete;
    glTFRenderPassBase(glTFRenderPassBase&&) = delete;

    glTFRenderPassBase& operator=(const glTFRenderPassBase&) = delete;
    glTFRenderPassBase& operator=(glTFRenderPassBase&&) = delete;

    // Which material should process within render pass
    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material) {return false;} 
    
    virtual ~glTFRenderPassBase() = 0;
    virtual const char* PassName() = 0;
    virtual bool InitPass(glTFRenderResourceManager& resourceManager);
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager);

    IRHIPipelineStateObject& GetPSO() const;
    
protected:
    virtual bool SetupMainDescriptorHeap(glTFRenderResourceManager& resourceManager) = 0;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) = 0;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) = 0;

    std::shared_ptr<IRHIRootSignature> m_rootSignature;
    
    std::shared_ptr<IRHIPipelineStateObject> m_pipelineStateObject;
    
    // CBV_SRV_UAV Heaps, can only bind one in render pass
    std::shared_ptr<IRHIDescriptorHeap> m_mainDescriptorHeap;
};
