#pragma once
#include "RHIInterface/IRHIBuffer.h"
#include "VolkUtils.h"

class VKBuffer : public IRHIBuffer
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKBuffer)
    
    bool InitBuffer(VkBuffer buffer, const RHIBufferDesc& desc);
    VkBuffer GetRawBuffer() const;

protected:
    VkBuffer m_buffer {VK_NULL_HANDLE};
};
