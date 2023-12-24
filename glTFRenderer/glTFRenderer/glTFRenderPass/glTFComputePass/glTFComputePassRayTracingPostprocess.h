#pragma once
#include "glTFComputePassBase.h"

struct RWTextureResourceWithBackBuffer
{
    RWTextureResourceWithBackBuffer(std::string output_register_name, std::string back_register_name);
    
    bool CreateResource(glTFRenderResourceManager& resource_manager, const IRHIRenderTargetDesc& desc);
    bool CreateDescriptors(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& main_descriptor);
    bool RegisterSignature(IRHIRootSignatureHelper& root_signature);
    bool AddShaderMacros(RHIShaderPreDefineMacros& macros);
    bool BindRootParameter(glTFRenderResourceManager& resource_manager);
    bool CopyToBackBuffer(glTFRenderResourceManager& resource_manager);

protected:
    std::string m_output_register_name;
    std::string m_back_register_name;
    
    std::string GetOutputBufferResourceName() const;
    std::string GetBackBufferResourceName() const;
    
    IRHIRenderTargetDesc m_texture_desc;
    
    std::shared_ptr<IRHIRenderTarget> m_writable_buffer;
    std::shared_ptr<IRHIRenderTarget> m_back_buffer;

    RHIGPUDescriptorHandle m_writable_buffer_handle;
    RHIGPUDescriptorHandle m_back_buffer_handle;
    
    RootSignatureAllocation m_writable_buffer_allocation;
    RootSignatureAllocation m_back_buffer_allocation;
};

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
    
    RWTextureResourceWithBackBuffer m_accumulation_resource;
    RWTextureResourceWithBackBuffer m_custom_resource;

    RootSignatureAllocation m_process_output_allocation;
    RootSignatureAllocation m_process_input_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;

    RHIGPUDescriptorHandle m_post_process_output_handle;
    RHIGPUDescriptorHandle m_post_process_input_handle;
    RHIGPUDescriptorHandle m_screen_uv_offset_handle;
};

