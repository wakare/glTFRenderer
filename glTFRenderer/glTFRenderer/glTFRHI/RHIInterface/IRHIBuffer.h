#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIBuffer : public IRHIResource
{
    friend class IRHIMemoryManager;
    friend class DX12MemoryManager;
    
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIBuffer)
    
    const RHIBufferDesc& GetBufferDesc() const {return m_buffer_desc; }
    
protected:
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) = 0;
    virtual bool UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size) = 0;
    
    RHIBufferDesc m_buffer_desc {};
};