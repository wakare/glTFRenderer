#pragma once
#include "glTFRHI/RHIInterface/IRHIVertexBufferView.h"

class VKVertexBufferView : public IRHIVertexBufferView
{
public:
    virtual void InitVertexBufferView(IRHIBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) override;
};
