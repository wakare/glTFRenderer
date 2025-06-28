#pragma once
#include "RHIInterface/IRHIResource.h"

class IRHIBuffer;

struct RHIIndexBufferViewDesc
{
    size_t size;
    size_t offset;
    RHIDataFormat format;
};

class RHICORE_API IRHIIndexBufferView
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIIndexBufferView)
    
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) = 0;
    
    RHIIndexBufferViewDesc m_desc{};
};
