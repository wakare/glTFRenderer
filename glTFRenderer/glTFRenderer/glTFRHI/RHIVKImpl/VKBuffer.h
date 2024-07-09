#pragma once
#include "glTFRHI/RHIInterface/IRHIBuffer.h"
#include <vulkan/vulkan_core.h>

class VKBuffer : public IRHIBuffer
{
public:
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) override;
    virtual bool UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size) override;

protected:
    VkDevice m_device {};
    VkBuffer m_buffer {};
    RHIBufferDesc m_desc {};
};
