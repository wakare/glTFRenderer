#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKGraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject
{
public:
    VKGraphicsPipelineStateObject() = default;
    virtual ~VKGraphicsPipelineStateObject() override;
    DECLARE_NON_COPYABLE(VKGraphicsPipelineStateObject)
    
    virtual bool InitPipelineStateObject(IRHIDevice& device, const RHIPipelineStateInfo& pipeline_state_info) override;
    virtual bool BindRenderTargetFormats(const std::vector<IRHIRenderTarget*>& render_targets) override;
    virtual bool BindRenderTargetFormats(const std::vector<IRHIDescriptorAllocation*>& render_targets) override;

    VkPipeline GetPipeline() const;
    VkPipelineLayout GetPipelineLayout() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
};
