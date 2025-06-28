#pragma once
#include "RHIInterface/IRHIDevice.h"
#include "RHIInterface/IRHIResource.h"

class RHICORE_API IRHISemaphore : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHISemaphore)

    virtual bool InitSemaphore(IRHIDevice& device) = 0;
};

class RHICORE_API RHISemaphoreNull : public IRHISemaphore
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RHISemaphoreNull)
    
    virtual bool InitSemaphore(IRHIDevice& device) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
};