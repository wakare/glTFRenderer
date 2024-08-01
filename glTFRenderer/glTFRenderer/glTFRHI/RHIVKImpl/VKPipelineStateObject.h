#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKGraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject
{
public:
    VKGraphicsPipelineStateObject() = default;
    virtual ~VKGraphicsPipelineStateObject() override;
    DECLARE_NON_COPYABLE(VKGraphicsPipelineStateObject)
    
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::vector<RHIPipelineInputLayout>& input_layouts) override;
    virtual bool BindRenderTargetFormats(const std::vector<IRHIDescriptorAllocation*>& render_targets) override;

    VkPipeline GetPipeline() const;
    VkPipelineLayout GetPipelineLayout() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};

    std::vector<VkFormat> m_bind_render_target_formats;
    VkFormat m_bind_depth_stencil_format;
};
