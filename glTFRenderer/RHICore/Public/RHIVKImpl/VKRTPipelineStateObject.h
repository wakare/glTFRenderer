#pragma once
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "VolkUtils.h"

#include <unordered_map>

class RHICORE_API VKRTPipelineStateObject : public IRHIRayTracingPipelineStateObject
{
public:
    VKRTPipelineStateObject() = default;
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::map<RHIShaderType,
                                         std::shared_ptr<IRHIShader>>& shaders) override;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;

    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipeline_layout; }
    uint32_t GetShaderGroupIndex(const std::string& name) const;
    uint32_t GetShaderGroupCount() const { return static_cast<uint32_t>(m_shader_groups.size()); }

protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkShaderModule m_shader_module {VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shader_groups;
    std::unordered_map<std::string, uint32_t> m_group_name_to_index;
};
