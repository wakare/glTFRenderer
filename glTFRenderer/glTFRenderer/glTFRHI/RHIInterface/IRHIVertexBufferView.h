#pragma once
#include "IRHIBuffer.h"
#include "IRHIResource.h"

class IRHIVertexBufferView : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIVertexBufferView)
    
    virtual void InitVertexBufferView(IRHIBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) = 0;
};
