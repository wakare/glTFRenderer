#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHICommandAllocator : public IRHIResource
{
public:
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) = 0;
};
