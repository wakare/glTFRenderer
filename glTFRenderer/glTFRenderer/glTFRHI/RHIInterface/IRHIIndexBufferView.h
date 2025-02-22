#pragma once
#include "IRHIResource.h"

class IRHIBuffer;

struct RHIIndexBufferViewDesc
{
    size_t size;
    size_t offset;
    RHIDataFormat format;
};

class IRHIIndexBufferView
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIIndexBufferView)
    
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) = 0;
    
    RHIIndexBufferViewDesc m_desc{};
};
