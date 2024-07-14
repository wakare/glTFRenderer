#pragma once
#include <vulkan/vulkan_core.h>
#include "glTFRHI/RHIInterface/IRHIDescriptorManager.h"

class VKBufferDescriptorAllocation : public IRHIBufferDescriptorAllocation
{
public:
    virtual bool InitFromBuffer(const IRHIBuffer& buffer, const RHIBufferDescriptorDesc& desc) override;
    VkBuffer GetRawBuffer() const;
    
protected:
    bool m_buffer_init {false};
    VkBuffer m_buffer {VK_NULL_HANDLE};
};

class VKTextureDescriptorAllocation : public IRHITextureDescriptorAllocation
{
protected:
    bool m_texture_init {false};
};

class VKDescriptorTable : public IRHIDescriptorTable
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations) override;
};

class VKDescriptorManager : public IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKDescriptorManager)

    virtual bool Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity);
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer, const RHIBufferDescriptorDesc& desc, std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHITexture& texture, const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIRenderTarget& texture, const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation) override;

    virtual bool BindDescriptors(IRHICommandList& command_list) override;
    virtual bool BindGUIDescriptors(IRHICommandList& command_list) override;

};
