#pragma once
#include "IRHIDevice.h"
#include "IRHIFactory.h"
#include "IRHIResource.h"

class IRHIMemoryAllocator : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIMemoryAllocator)
    
    virtual bool InitMemoryAllocator(const IRHIFactory& factory, const IRHIDevice& device) = 0;
    virtual void DestroyMemoryAllocator() = 0;
};
