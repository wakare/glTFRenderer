#pragma once
#include "glTFComputePassBase.h"

class glTFComputePassRayTracingPostprocess : public glTFComputePassBase
{
public:
    glTFComputePassRayTracingPostprocess();

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

    DispatchCount m_dispatch_count;

    std::shared_ptr<IRHIRenderTarget> m_post_process_input_RT;
    std::shared_ptr<IRHIRenderTarget> m_screen_uv_offset_RT;
    std::shared_ptr<IRHIRenderTarget> m_post_process_output_RT;

    glTFRenderResourceUtils::RWTextureResourceWithBackBuffer m_accumulation_resource;
    glTFRenderResourceUtils::RWTextureResourceWithBackBuffer m_custom_resource;

    RootSignatureAllocation m_process_output_allocation;
    RootSignatureAllocation m_process_input_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;

    RHIGPUDescriptorHandle m_post_process_output_handle;
    RHIGPUDescriptorHandle m_post_process_input_handle;
    RHIGPUDescriptorHandle m_screen_uv_offset_handle;
};

