#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIGPUBufferManager;

class IRHIGPUBuffer : public IRHIResource
{
public:
    IRHIGPUBuffer();
    
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) = 0;
    virtual bool UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size) = 0;
    virtual GPU_BUFFER_HANDLE_TYPE GetGPUBufferHandle() = 0;

    const RHIBufferDesc& GetBufferDesc() const {return m_buffer_desc; }
    
protected:
    RHIBufferDesc m_buffer_desc;
};