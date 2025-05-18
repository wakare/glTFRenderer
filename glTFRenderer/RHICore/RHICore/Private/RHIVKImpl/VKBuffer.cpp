#include "VKBuffer.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"

bool VKBuffer::InitBuffer(VkBuffer buffer, const RHIBufferDesc& desc)
{
    m_buffer = buffer;
    m_buffer_desc = desc;
    need_release = true;
    
    return true;
}

VkBuffer VKBuffer::GetRawBuffer() const
{
    GLTF_CHECK(need_release);
    return m_buffer;
}