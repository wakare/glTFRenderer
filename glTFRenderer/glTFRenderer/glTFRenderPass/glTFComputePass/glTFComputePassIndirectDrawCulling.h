#pragma once
#include "glTFComputePassBase.h"

class IRHIBufferAllocation;

class glTFComputePassIndirectDrawCulling : public glTFComputePassBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFComputePassIndirectDrawCulling)
    
    virtual const char* PassName() override;

    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual DispatchCount GetDispatchCount(glTFRenderResourceManager& resource_manager) const override;
    
protected:
    DispatchCount m_dispatch_count{};
    RootSignatureAllocation m_culled_indirect_command_allocation;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_culled_command_output_descriptor;
    
    RootSignatureAllocation m_culled_count_output_allocation;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_culled_count_output_descriptor;
};
