#pragma once
#include "IRHIGPUBuffer.h"
#include "IRHIResource.h"

class IRHIVertexBufferView : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIVertexBufferView)
    
    virtual void InitVertexBufferView(IRHIGPUBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) = 0;

    virtual RHIGPUDescriptorHandle GetGPUHandle() const = 0;
    virtual size_t GetSize() const = 0;
    virtual size_t GetStride() const = 0;
};

struct RHIIndirectArgumentVertexBufferView
{
    RHIIndirectArgumentVertexBufferView(const IRHIVertexBufferView& vertex_buffer_view)
        : handle(vertex_buffer_view.GetGPUHandle())
        , size(vertex_buffer_view.GetSize())
        , stride(vertex_buffer_view.GetStride())
    
    {
        
    }
    
    RHIGPUDescriptorHandle handle;
    unsigned size;
    unsigned stride;
};