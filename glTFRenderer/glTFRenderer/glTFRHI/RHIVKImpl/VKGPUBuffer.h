#pragma once
#include "glTFRHI/RHIInterface/IRHIGPUBuffer.h"

class VKGPUBuffer : public IRHIGPUBuffer
{
public:
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) override;
    virtual bool UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size) override;
    virtual GPU_BUFFER_HANDLE_TYPE GetGPUBufferHandle() override;
};
