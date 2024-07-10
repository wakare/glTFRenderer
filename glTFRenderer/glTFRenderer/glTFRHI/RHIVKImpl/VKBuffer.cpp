#include "VKBuffer.h"

bool VKBuffer::InitBuffer(VkDevice device, VkBuffer buffer, const RHIBufferDesc& desc)
{
    m_device = device;
    m_buffer = buffer;
    m_buffer_desc = desc;
    
    return true;
}
