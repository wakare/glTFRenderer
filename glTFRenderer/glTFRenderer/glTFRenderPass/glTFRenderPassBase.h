#pragma once
#include "glTFRenderResourceManager.h"
#include "../glTFMaterial/glTFMaterialBase.h"
#include "../glTFScene/glTFSceneObjectBase.h"

struct RHIPipelineInputLayout;
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

    virtual ~glTFRenderPassBase() = 0;
    virtual const char* PassName() = 0;
    
    // Which material should process within render pass
    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material);
    virtual bool InitPass(glTFRenderResourceManager& resourceManager);
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager);

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) = 0;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resourceManager) {return true; }
    
    IRHIPipelineStateObject& GetPSO() const;

    void SetByPass(bool bypass) { m_bypass = bypass; }
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetMainDescriptorHeapSize() = 0;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) = 0;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) = 0;
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() = 0;
    
    std::shared_ptr<IRHIRootSignature> m_root_signature;
    
    std::shared_ptr<IRHIPipelineStateObject> m_pipeline_state_object;
    
    // CBV_SRV_UAV Heaps, can only bind one in render pass
    std::shared_ptr<IRHIDescriptorHeap> m_main_descriptor_heap;

    // Bypass this pass which determined by scene view?
    bool m_bypass {false};
};
