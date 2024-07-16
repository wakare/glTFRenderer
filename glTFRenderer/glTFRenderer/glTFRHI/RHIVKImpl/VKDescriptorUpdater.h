#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"

class IRHIRootSignature;

class VKDescriptorUpdater : public IRHIDescriptorUpdater
{
public:
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorAllocation& allocation) override;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorTable& allocation_table) override;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) override;

protected:
    std::vector<VkDescriptorBufferInfo> m_cache_buffer_infos;
    std::vector<VkDescriptorImageInfo> m_cache_image_infos;
    std::vector<VkWriteDescriptorSet> m_cache_descriptor_writers;
};
