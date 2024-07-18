#pragma once
#include "IRHIResource.h"

class IRHIBuffer;

struct RHIIndexBufferViewDesc
{
    size_t size;
    size_t offset;
    RHIDataFormat format;
};

class IRHIIndexBufferView : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIIndexBufferView)
    
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) = 0;
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const = 0;
    virtual size_t GetSize() const = 0;
    virtual unsigned GetFormat() const = 0;

    RHIIndexBufferViewDesc m_desc{};
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