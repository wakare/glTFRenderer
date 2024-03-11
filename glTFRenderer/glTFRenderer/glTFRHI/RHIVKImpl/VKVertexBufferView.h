#pragma once
#include "glTFRHI/RHIInterface/IRHIVertexBufferView.h"

class VKVertexBufferView : public IRHIVertexBufferView
{
public:
    virtual void InitVertexBufferView(IRHIGPUBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) override;

    virtual RHIGPUDescriptorHandle GetGPUHandle() const override;
    virtual size_t GetSize() const override;
    virtual size_t GetStride() const override;
};
