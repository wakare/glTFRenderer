#pragma once
#include "glTFComputePassBase.h"

class glTFComputePassReSTIRDirectLighting : public glTFComputePassBase
{
public:
    glTFComputePassReSTIRDirectLighting();
    
    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount() const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

private:
    DispatchCount m_dispatch_count;
    
    std::shared_ptr<IRHIRenderTarget> m_output;
    RootSignatureAllocation m_output_allocation;
    RHIGPUDescriptorHandle m_output_handle;

    glTFRenderResourceUtils::RWTextureResourceWithBackBuffer m_aggregate_samples_output;
    
    // External resource
    std::shared_ptr<IRHIRenderTarget> m_lighting_samples;
    std::shared_ptr<IRHIRenderTarget> m_screen_uv_offset;

    RHIGPUDescriptorHandle m_lighting_samples_handle;
    RHIGPUDescriptorHandle m_screen_uv_offset_handle;
    
    RootSignatureAllocation m_lighting_samples_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;
};
