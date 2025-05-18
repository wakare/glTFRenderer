#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHISemaphore : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHISemaphore)

    virtual bool InitSemaphore(IRHIDevice& device) = 0;
};

class RHISemaphoreNull : public IRHISemaphore
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RHISemaphoreNull)
    
    virtual bool InitSemaphore(IRHIDevice& device) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
};