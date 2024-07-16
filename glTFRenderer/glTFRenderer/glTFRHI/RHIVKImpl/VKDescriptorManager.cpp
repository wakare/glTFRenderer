#include "VKDescriptorManager.h"

#include "VKBuffer.h"

bool VKBufferDescriptorAllocation::InitFromBuffer(const IRHIBuffer& buffer, const RHIBufferDescriptorDesc& desc)
{
    m_view_desc = desc;
    m_buffer = dynamic_cast<const VKBuffer&>(buffer).GetRawBuffer();
    m_buffer_init = true;
    return true;
}

VkBuffer VKBufferDescriptorAllocation::GetRawBuffer() const
{
    GLTF_CHECK(m_buffer_init);
    return m_buffer;
}

VkImageView VKTextureDescriptorAllocation::GetRawImageView() const
{
    return m_image_view;
}

bool VKDescriptorTable::Build(IRHIDevice& device,
                              const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations)
{
    return false;
}

bool VKDescriptorManager::Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer, const RHIBufferDescriptorDesc& desc,
                                           std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHITexture& texture, const RHITextureDescriptorDesc& desc,
                                           std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation)
{
    return false;
}

bool VKDescriptorManager::BindDescriptors(IRHICommandList& command_list)
{
    return false;
}

bool VKDescriptorManager::BindGUIDescriptors(IRHICommandList& command_list)
{
    return false;
}
