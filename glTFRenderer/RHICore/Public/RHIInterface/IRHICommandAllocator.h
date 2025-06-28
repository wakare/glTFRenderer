#pragma once
#include "RHIInterface/IRHIDevice.h"
#include "RHIInterface/IRHIResource.h"

class RHICORE_API IRHICommandAllocator : public IRHIResource
{
public:
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) = 0;
};
