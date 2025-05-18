#pragma once
#include "VolkUtils.h"
#include "IRHIDescriptorUpdater.h"

class VKDescriptorUpdater : public IRHIDescriptorUpdater
{
public:
    virtual bool BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation) override;
    virtual bool BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorTable& allocation_table, RHIDescriptorRangeType
                                            descriptor_type) override;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) override;

protected:
    std::list<VkDescriptorBufferInfo> m_cache_buffer_infos;
    std::list<VkDescriptorImageInfo> m_cache_image_infos;
    std::map<unsigned, std::vector<VkWriteDescriptorSet>> m_cache_descriptor_writers;
};
