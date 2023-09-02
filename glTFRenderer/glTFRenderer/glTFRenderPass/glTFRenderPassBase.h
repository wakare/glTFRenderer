#pragma once
#include "glTFRenderResourceManager.h"
#include "../glTFScene/glTFSceneObjectBase.h"

class glTFMaterialBase;
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
    virtual bool InitPass(glTFRenderResourceManager& resource_manager);
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager);
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager);
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager);
    virtual std::shared_ptr<IRHIPipelineStateObject> GetPSO() const = 0;

    // Which material should process within render pass
    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material) {return true; }
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) {return true; }
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resourceManager) {return true; }
    
    void SetByPass(bool bypass) { m_bypass = bypass; }
    
protected:
    // Must be implement in final render pass class
    virtual size_t GetMainDescriptorHeapSize() = 0;
    virtual size_t GetRootSignatureParameterCount() = 0;
    virtual size_t GetRootSignatureSamplerCount() = 0;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) = 0;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) = 0;
    
    std::shared_ptr<IRHIRootSignature> m_root_signature;
    
    // CBV_SRV_UAV Heaps, can only bind one in render pass
    std::shared_ptr<IRHIDescriptorHeap> m_main_descriptor_heap;

    // Bypass this pass which determined by scene view?
    bool m_bypass {false};
};
