#pragma once
#include "glTFRHI/RHIInterface/IRHIBuffer.h"
#include "VolkUtils.h"

class VKBuffer : public IRHIBuffer
{
public:
    virtual ~VKBuffer() override;
    
    bool InitBuffer(VkDevice device, VkBuffer buffer, const RHIBufferDesc& desc);
    VkBuffer GetRawBuffer() const;
    
protected:
    bool m_buffer_init {false};
    VkDevice m_device {VK_NULL_HANDLE};
    VkBuffer m_buffer {VK_NULL_HANDLE};
};
