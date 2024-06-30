#pragma once
#include "IRHIResource.h"

class IRHIBuffer;

class IRHIIndexBufferView : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIIndexBufferView)
    
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, size_t offset, RHIDataFormat indexFormat, size_t indexBufferSize) = 0;
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const = 0;
    virtual size_t GetSize() const = 0;
    virtual unsigned GetFormat() const = 0;
};

struct RHIIndirectArgumentIndexBufferView
{
    RHIIndirectArgumentIndexBufferView(const IRHIIndexBufferView& index_buffer_view)
        : handle(index_buffer_view.GetGPUHandle())
        , size(index_buffer_view.GetSize())
        , format(index_buffer_view.GetFormat())
    {
        
    }
    
    RHIGPUDescriptorHandle handle;
    unsigned size;
    unsigned format; 
};