#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"

class RHICORE_API IRHICommandAllocator : public IRHIResource
{
public:
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) = 0;
};
