#include "VKGPUBuffer.h"
#include "VKDevice.h"

bool VKGPUBuffer::InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    m_desc = desc;
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
    };
    switch (desc.resource_type) {
    case RHIBufferResourceType::Buffer:
        bufferInfo.size = desc.width;
        break;
    case RHIBufferResourceType::Tex1D:
        break;
    case RHIBufferResourceType::Tex2D:
        break;
    case RHIBufferResourceType::Tex3D:
        break;
    }

    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (desc.usage & RUF_ALLOW_UAV)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_buffer);
    return true;
}

bool VKGPUBuffer::UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size)
{
    //find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = m_buffer };
    VkDeviceAddress buffer_device_address = vkGetBufferDeviceAddress(m_device, &deviceAdressInfo);

    return true;
}

unsigned long long VKGPUBuffer::GetGPUBufferHandle()
{
    return 0;
}
