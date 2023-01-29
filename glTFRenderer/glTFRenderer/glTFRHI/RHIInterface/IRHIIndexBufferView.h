#pragma once
#include "IRHIResource.h"

class IRHIGPUBuffer;

class IRHIIndexBufferView : public IRHIResource
{
public:
    virtual bool InitIndexBufferView(IRHIGPUBuffer& buffer, size_t offset, RHIDataFormat indexFormat, size_t indexBufferSize) = 0;
};
