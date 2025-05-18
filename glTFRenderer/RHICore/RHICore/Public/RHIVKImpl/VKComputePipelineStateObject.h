#pragma once
#include "VolkUtils.h"
#include "IRHIPipelineStateObject.h"

class VKComputePipelineStateObject : public IRHIComputePipelineStateObject
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKComputePipelineStateObject)

    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    VkPipeline GetPipeline() const;
    VkPipelineLayout GetPipelineLayout() const;
    
protected:
    VkShaderModule CreateVkShaderModule(VkDevice device, const std::vector<unsigned char>& shader_binaries);
    
    VkDevice m_device{VK_NULL_HANDLE};
    VkShaderModule m_compute_shader_module{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
};
