#include "VKMemoryAllocator.h"

#include "VKDevice.h"
#include "VKFactory.h"

//#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

bool VKMemoryAllocator::InitMemoryAllocator(const IRHIFactory& factory, const IRHIDevice& device)
{
    const VKFactory& vk_factory = dynamic_cast<const VKFactory&>(factory);
    const VKDevice& vk_device = dynamic_cast<const VKDevice&>(device);

    VmaAllocatorCreateInfo vma_allocator_create_info{};
    vma_allocator_create_info.physicalDevice = vk_device.GetPhysicalDevice();
    vma_allocator_create_info.device = vk_device.GetDevice();
    vma_allocator_create_info.instance = vk_factory.GetInstance();
    vma_allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    
    VmaVulkanFunctions vma_vulkan_func{};
    vma_vulkan_func.vkAllocateMemory                    = vkAllocateMemory;
    vma_vulkan_func.vkBindBufferMemory                  = vkBindBufferMemory;
    vma_vulkan_func.vkBindImageMemory                   = vkBindImageMemory;
    vma_vulkan_func.vkCreateBuffer                      = vkCreateBuffer;
    vma_vulkan_func.vkCreateImage                       = vkCreateImage;
    vma_vulkan_func.vkDestroyBuffer                     = vkDestroyBuffer;
    vma_vulkan_func.vkDestroyImage                      = vkDestroyImage;
    vma_vulkan_func.vkFlushMappedMemoryRanges           = vkFlushMappedMemoryRanges;
    vma_vulkan_func.vkFreeMemory                        = vkFreeMemory;
    vma_vulkan_func.vkGetBufferMemoryRequirements       = vkGetBufferMemoryRequirements;
    vma_vulkan_func.vkGetImageMemoryRequirements        = vkGetImageMemoryRequirements;
    vma_vulkan_func.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vma_vulkan_func.vkGetPhysicalDeviceProperties       = vkGetPhysicalDeviceProperties;
    vma_vulkan_func.vkInvalidateMappedMemoryRanges      = vkInvalidateMappedMemoryRanges;
    vma_vulkan_func.vkMapMemory                         = vkMapMemory;
    vma_vulkan_func.vkUnmapMemory                       = vkUnmapMemory;
    vma_vulkan_func.vkCmdCopyBuffer                     = vkCmdCopyBuffer;
    vma_vulkan_func.vkGetInstanceProcAddr               = vkGetInstanceProcAddr;
    vma_vulkan_func.vkGetDeviceProcAddr                 = vkGetDeviceProcAddr;
    
    vma_allocator_create_info.pVulkanFunctions = &vma_vulkan_func;
    VkResult result = vmaCreateAllocator(&vma_allocator_create_info, &m_vma_allocator);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

VmaAllocator VKMemoryAllocator::GetAllocator() const
{
    return m_vma_allocator;
}
