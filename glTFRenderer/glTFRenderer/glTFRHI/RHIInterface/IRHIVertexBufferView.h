#pragma once
#include "IRHIGPUBuffer.h"
#include "IRHIResource.h"

class IRHIVertexBufferView : public IRHIResource
{
public:
    virtual void InitVertexBufferView(IRHIGPUBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) = 0;
};
