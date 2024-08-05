#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKGraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR(VKGraphicsPipelineStateObject)
    virtual ~VKGraphicsPipelineStateObject() override;
    
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::vector<RHIPipelineInputLayout>& input_layouts) override;
    virtual bool BindRenderTargetFormats(const std::vector<IRHIDescriptorAllocation*>& render_targets) override;

    VkPipeline GetPipeline() const;
    VkPipelineLayout GetPipelineLayout() const;
    
protected:
    VkShaderModule CreateVkShaderModule(VkDevice device, const std::vector<unsigned char>& shader_binaries);
    
    VkDevice m_device{VK_NULL_HANDLE};
    VkShaderModule m_vertex_shader_module{VK_NULL_HANDLE};
    VkShaderModule m_fragment_shader_module{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};

    std::vector<VkFormat> m_bind_render_target_formats;
    VkFormat m_bind_depth_stencil_format;
};
