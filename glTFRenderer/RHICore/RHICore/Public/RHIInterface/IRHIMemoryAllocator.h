#pragma once
#include "IRHIDevice.h"
#include "IRHIFactory.h"
#include "IRHIResource.h"

class RHICORE_API IRHIMemoryAllocator : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIMemoryAllocator)
    
    virtual bool InitMemoryAllocator(const IRHIFactory& factory, const IRHIDevice& device) = 0;
};
