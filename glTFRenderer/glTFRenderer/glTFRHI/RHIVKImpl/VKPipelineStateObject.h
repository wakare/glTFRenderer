#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKGraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject
{
public:
    DECLARE_NON_COPYABLE(VKGraphicsPipelineStateObject)
    
    virtual bool InitPipelineStateObject(IRHIDevice& device, const RHIPipelineStateInfo& pipeline_state_info) override;
    virtual bool BindRenderTargetFormats(const std::vector<IRHIRenderTarget*>& render_targets) override;

    const VkPipeline& GetPipeline() const;
    
protected:
    VkPipelineLayout m_pipeline_layout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
};
