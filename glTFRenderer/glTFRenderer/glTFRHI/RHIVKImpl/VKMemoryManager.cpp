#include "VKMemoryManager.h"

#include "VKBuffer.h"
#include "VKDevice.h"
#include "VKConverterUtils.h"
#include "VKTexture.h"
#include "VulkanUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "VKCommandList.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"

bool VKMemoryManager::InitMemoryManager(IRHIDevice& device, const IRHIFactory& factory,
                                        const DescriptorAllocationInfo& max_descriptor_capacity)
{
    RETURN_IF_FALSE(IRHIMemoryManager::InitMemoryManager(device, factory, max_descriptor_capacity))

    m_allocator = RHIResourceFactory::CreateRHIResource<IRHIMemoryAllocator>();
    return m_allocator->InitMemoryAllocator(factory, device);
}

bool VKMemoryManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
                                           std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    GLTF_CHECK(buffer_desc.usage);
    
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = buffer_desc.width;
    bufferInfo.usage = VKConverterUtils::ConvertToBufferUsage(buffer_desc.usage); 
    
    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = buffer_desc.type == RHIBufferType::Upload ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;
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
    out_buffer_allocation->SetNeedRelease();

    VkDevice vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    dynamic_cast<VKBuffer&>(*buffer_allocation.m_buffer).InitBuffer(out_buffer, buffer_desc);

    m_buffer_allocations.push_back(out_buffer_allocation);
    
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

bool VKMemoryManager::AllocateTextureMemory(IRHIDevice& device, glTFRenderResourceManager& resource_manager,
                                            const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation)
{
    VkImageCreateInfo image_create_info {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = nullptr};

    image_create_info.format = VKConverterUtils::ConvertToFormat(texture_desc.GetDataFormat());
    image_create_info.extent = {texture_desc.GetTextureWidth(), texture_desc.GetTextureHeight(), 1};
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.usage = VKConverterUtils::ConvertToImageUsage(texture_desc.GetUsage());
    
    VmaAllocationCreateInfo draw_image_allocation_create_info {};
    draw_image_allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    draw_image_allocation_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImage out_image;
    VmaAllocation out_allocation;
    VmaAllocationInfo out_allocation_info;
    
    VK_CHECK(vmaCreateImage(GetVmaAllocator(), &image_create_info, &draw_image_allocation_create_info, &out_image, &out_allocation, &out_allocation_info))

    out_texture_allocation = std::make_shared<VKTextureAllocation>();
    auto& vk_texture_allocation = dynamic_cast<VKTextureAllocation&>(*out_texture_allocation);
    vk_texture_allocation.m_allocation = out_allocation;
    vk_texture_allocation.m_allocation_info = out_allocation_info;

    vk_texture_allocation.m_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    GLTF_CHECK(dynamic_cast<VKTexture&>(*vk_texture_allocation.m_texture).Init(dynamic_cast<VKDevice&>(device).GetDevice(), out_image, texture_desc));

    m_texture_allocations.push_back(out_texture_allocation);
    
    return true;
}

bool VKMemoryManager::AllocateTextureMemoryAndUpload(IRHIDevice& device, glTFRenderResourceManager& resource_manager,
                                                     IRHICommandList& command_list, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation)
{
    AllocateTextureMemory(device, resource_manager, texture_desc, out_texture_allocation);

    auto& memory_manager = resource_manager.GetMemoryManager();
    RHIBufferDesc upload_buffer_desc{};
    upload_buffer_desc.width = texture_desc.GetTextureDataSize();
    upload_buffer_desc.height = 1;
    upload_buffer_desc.depth = 1;
    upload_buffer_desc.type = RHIBufferType::Upload;
    upload_buffer_desc.resource_type = RHIBufferResourceType::Buffer;
    upload_buffer_desc.usage = RUF_TRANSFER_SRC;

    std::shared_ptr<IRHIBufferAllocation> buffer_allocation;
    memory_manager.AllocateBufferMemory(device, upload_buffer_desc, buffer_allocation);
    memory_manager.UploadBufferData(*buffer_allocation, texture_desc.GetTextureData(), 0, texture_desc.GetTextureDataSize());

    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto vk_buffer = dynamic_cast<VKBuffer&>(*buffer_allocation->m_buffer).GetRawBuffer();
    auto vk_image = dynamic_cast<VKTexture&>(*out_texture_allocation->m_texture).GetRawImage();

    out_texture_allocation->m_texture->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    auto vk_image_layout = VKConverterUtils::ConvertToImageLayout(out_texture_allocation->m_texture->GetState());
    
    VkBufferImageCopy copy_info{};
    copy_info.bufferOffset = 0;
    copy_info.bufferImageHeight = 0;
    copy_info.bufferRowLength = 0;
    copy_info.imageOffset = VkOffset3D(0.0f, 0.0f, 0.0f);
    copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_info.imageSubresource.layerCount = 1;
    copy_info.imageSubresource.mipLevel = 0;
    copy_info.imageSubresource.baseArrayLayer = 0;
    copy_info.imageExtent = VkExtent3D(texture_desc.GetTextureWidth(), texture_desc.GetTextureHeight(), 1);
    vkCmdCopyBufferToImage(vk_command_buffer, vk_buffer, vk_image, vk_image_layout, 1, &copy_info);
    
    resource_manager.CloseCurrentCommandListAndExecute({}, true);
    
    return true;
}

bool VKMemoryManager::ReleaseMemoryAllocation(glTFRenderResourceManager& resource_manager,
    IRHIMemoryAllocation& memory_allocation)
{
    const auto* raw_pointer = &memory_allocation;

    switch (memory_allocation.GetAllocationType()) {
    case IRHIMemoryAllocation::BUFFER:
        {
            const auto& vk_buffer_allocation = dynamic_cast<const VKBufferAllocation&>(memory_allocation);
            VkBuffer vk_buffer = dynamic_cast<const VKBuffer&>(*vk_buffer_allocation.m_buffer).GetRawBuffer();
        
            vmaDestroyBuffer(GetVmaAllocator(), vk_buffer, vk_buffer_allocation.m_allocation);

            bool removed = false;
            for (auto iter = m_buffer_allocations.begin(); iter != m_buffer_allocations.end(); ++iter)
            {
                if (iter->get() == raw_pointer)
                {
                    m_buffer_allocations.erase(iter);
                    removed = true;
                    break;
                }
            }

            GLTF_CHECK(removed);
        }
        break;
    case IRHIMemoryAllocation::TEXTURE:
        {
            const auto& vk_texture_allocation = dynamic_cast<const VKTextureAllocation&>(memory_allocation);
            VkImage vk_image = dynamic_cast<const VKTexture&>(*vk_texture_allocation.m_texture).GetRawImage();

            vmaDestroyImage(GetVmaAllocator(), vk_image, vk_texture_allocation.m_allocation);

            bool removed = false;
            for (auto iter = m_texture_allocations.begin(); iter != m_texture_allocations.end(); ++iter)
            {
                if (iter->get() == raw_pointer)
                {
                    m_texture_allocations.erase(iter);
                    removed = true;
                    break;
                }
            }

            GLTF_CHECK(removed);
        }
        break;
    }

    return true;
}

bool VKMemoryManager::ReleaseAllResource(glTFRenderResourceManager& resource_manager)
{
    for (auto& buffer_allocation : m_buffer_allocations)
    {
        const auto& vk_buffer_allocation = dynamic_cast<const VKBufferAllocation&>(*buffer_allocation);
        VkBuffer vk_buffer = dynamic_cast<const VKBuffer&>(*vk_buffer_allocation.m_buffer).GetRawBuffer();
        
        vmaDestroyBuffer(GetVmaAllocator(), vk_buffer, vk_buffer_allocation.m_allocation);
    }

    for (const auto& texture_allocation : m_texture_allocations)
    {
        const auto& vk_texture_allocation = dynamic_cast<const VKTextureAllocation&>(*texture_allocation);
        VkImage vk_image = dynamic_cast<const VKTexture&>(*vk_texture_allocation.m_texture).GetRawImage();

        vmaDestroyImage(GetVmaAllocator(), vk_image, vk_texture_allocation.m_allocation);
    }
    
    m_allocator->Release(resource_manager);
    
    return true;
}

VmaAllocator VKMemoryManager::GetVmaAllocator() const
{
    return dynamic_cast<VKMemoryAllocator&>(*m_allocator).GetAllocator();
}

