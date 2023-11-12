#pragma once
#include "IRHIResource.h"

class IRHIGPUBuffer;

class IRHIIndexBufferView : public IRHIResource
{
public:
    virtual bool InitIndexBufferView(IRHIGPUBuffer& buffer, size_t offset, RHIDataFormat indexFormat, size_t indexBufferSize) = 0;
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const = 0;
    virtual size_t GetSize() const = 0;
    virtual unsigned GetFormat() const = 0;
};
