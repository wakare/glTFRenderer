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
    VkResult result = vmaCreateAllocator(&vma_allocator_create_info, &m_vma_allocator);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

VmaAllocator VKMemoryAllocator::GetAllocator() const
{
    return m_vma_allocator;
}
