#pragma once
#include "VolkUtils.h"
#include "IRHIDescriptorManager.h"

class IRHIFactory;

class VKBufferDescriptorAllocation : public IRHIBufferDescriptorAllocation
{
public:
    virtual bool InitFromBuffer(const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc) override;
    VkBuffer GetRawBuffer() const;
    
protected:
    bool m_buffer_init {false};
};

class VKTextureDescriptorAllocation : public IRHITextureDescriptorAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKTextureDescriptorAllocation)
    
    bool InitFromImageView(const std::shared_ptr<IRHITexture>& texture, VkDevice device, VkImageView image_view, const RHITextureDescriptorDesc& desc);
    
    VkImageView GetRawImageView() const;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    bool m_image_init {false};
    VkImageView m_image_view {VK_NULL_HANDLE};
    VkDevice m_device {VK_NULL_HANDLE};
};

class VKDescriptorTable : public IRHIDescriptorTable
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations) override;

    const std::vector<VkDescriptorImageInfo>& GetImageInfos() const;
    
protected:
    std::vector<VkDescriptorImageInfo> m_image_infos;
};

class VKDescriptorManager : public IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKDescriptorManager)

    virtual bool Init(IRHIDevice& device, const DescriptorAllocationInfo& max_descriptor_capacity) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc, std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHITexture>& texture, const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation) override;

    virtual bool BindDescriptorContext(IRHICommandList& command_list) override;
    virtual bool BindGUIDescriptorContext(IRHICommandList& command_list) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    VkDescriptorPool GetDescriptorPool() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkDescriptorPool m_descriptor_pool {VK_NULL_HANDLE};
};
