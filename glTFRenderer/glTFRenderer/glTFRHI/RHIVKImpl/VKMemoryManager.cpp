#include "VKMemoryManager.h"

#include "VKBuffer.h"
#include "VKDevice.h"
#include "VKConverterUtils.h"
#include "VKTexture.h"
#include "VulkanUtils.h"
#include "glTFRHI/RHIResourceFactory.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

bool VKMemoryManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = buffer_desc.width;

    if (buffer_desc.usage & RUF_ALLOW_UAV)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;    
    }
    
    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer out_buffer;
    VmaAllocation out_allocation;
    VmaAllocationInfo out_allocation_info;
    
    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(GetVmaAllocator(), &bufferInfo, &allocation_create_info, &out_buffer, &out_allocation,
        &out_allocation_info))

    out_buffer_allocation = std::make_shared<VKBufferAllocation>();
    auto& buffer_allocation = dynamic_cast<VKBufferAllocation&>(*out_buffer_allocation);
    buffer_allocation.m_allocation = out_allocation;
    buffer_allocation.m_allocation_info = out_allocation_info;

    buffer_allocation.m_buffer = RHIResourceFactory::CreateRHIResource<IRHIBuffer>();
    VkDevice vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    dynamic_cast<VKBuffer&>(*buffer_allocation.m_buffer).InitBuffer(vk_device, out_buffer, buffer_desc);

    return true;
}

bool VKMemoryManager::UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset,
    size_t size)
{
    auto vma_buffer_allocation = dynamic_cast<const VKBufferAllocation&>(buffer_allocation).m_allocation; 
    void* mapped_data = vma_buffer_allocation->GetMappedData();
    memcpy(mapped_data, (char*)data + offset, size);
    
    return true;
}

bool VKMemoryManager::AllocateTextureMemory(IRHIDevice& device, const RHITextureDesc& texture_desc,
    std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    VkImageCreateInfo image_create_info {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

    if (texture_desc.GetUsage() & RUF_ALLOW_UAV)
    {
        image_create_info.flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (texture_desc.GetUsage() & RUF_ALLOW_DEPTH_STENCIL)
    {
        image_create_info.flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (texture_desc.GetUsage() & RUF_ALLOW_RENDER_TARGET)
    {
        image_create_info.flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    image_create_info.format = VKConverterUtils::ConvertToFormat(texture_desc.GetDataFormat());
    image_create_info.extent = {texture_desc.GetTextureWidth(), texture_desc.GetTextureHeight(), 1};
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    VmaAllocationCreateInfo draw_image_allocation_create_info {};
    draw_image_allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    draw_image_allocation_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImage out_image;
    VmaAllocation out_allocation;
    VmaAllocationInfo out_allocation_info;
    
    VK_CHECK(vmaCreateImage(GetVmaAllocator(), &image_create_info, &draw_image_allocation_create_info, &out_image, &out_allocation, &out_allocation_info))

    out_buffer_allocation = std::make_shared<VKTextureAllocation>();
    auto& vk_buffer_allocation = dynamic_cast<VKTextureAllocation&>(*out_buffer_allocation);
    vk_buffer_allocation.m_allocation = out_allocation;
    vk_buffer_allocation.m_allocation_info = out_allocation_info;

    vk_buffer_allocation.m_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    GLTF_CHECK(dynamic_cast<VKTexture&>(*vk_buffer_allocation.m_texture).Init(dynamic_cast<VKDevice&>(device).GetDevice(), out_image));
    
    return true;
}

bool VKMemoryManager::AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list,
                                                     const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    return false;
}

bool VKMemoryManager::CleanAllocatedResource()
{
    return true;
}

VmaAllocator VKMemoryManager::GetVmaAllocator() const
{
    return dynamic_cast<VKMemoryAllocator&>(*m_memory_allocator).GetAllocator();
}
