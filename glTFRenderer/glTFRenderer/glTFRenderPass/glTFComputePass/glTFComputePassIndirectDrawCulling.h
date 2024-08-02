#pragma once
#include "glTFComputePassBase.h"

class glTFComputePassIndirectDrawCulling : public glTFComputePassBase
{
public:
    glTFComputePassIndirectDrawCulling();
    
    virtual const char* PassName() override;

    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool NeedRendering() const override;
    
    virtual DispatchCount GetDispatchCount() const override;
    
protected:
    virtual bool UpdateGUIWidgets() override;
    
    std::shared_ptr<IRHIBufferAllocation> m_count_reset_buffer;
    DispatchCount m_dispatch_count;
    RootSignatureAllocation m_culled_indirect_command_allocation;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_command_buffer_handle;
    
    bool m_enable_culling; 
};
