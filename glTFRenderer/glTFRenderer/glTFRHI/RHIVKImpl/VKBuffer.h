#pragma once
#include "glTFRHI/RHIInterface/IRHIBuffer.h"
#include <vulkan/vulkan_core.h>

class VKBuffer : public IRHIBuffer
{
public:
    bool InitBuffer(VkDevice device, VkBuffer buffer, const RHIBufferDesc& desc);
    VkBuffer GetRawBuffer() const;
    
protected:
    bool m_buffer_init {false};
    VkDevice m_device {VK_NULL_HANDLE};
    VkBuffer m_buffer {VK_NULL_HANDLE};
};
