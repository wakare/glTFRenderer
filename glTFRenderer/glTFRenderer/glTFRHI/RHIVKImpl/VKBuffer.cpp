#include "VKBuffer.h"

bool VKBuffer::InitBuffer(VkDevice device, VkBuffer buffer, const RHIBufferDesc& desc)
{
    m_device = device;
    m_buffer = buffer;
    m_buffer_desc = desc;
    m_buffer_init = true;
    
    return true;
}

VkBuffer VKBuffer::GetRawBuffer() const
{
    GLTF_CHECK(m_buffer_init);
    return m_buffer;
}
