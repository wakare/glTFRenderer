#pragma once
#include "IRHIDevice.h"
#include "../IRHIResource.h"

enum class RHICommandAllocatorType
{
    DIRECT,
    COMPUTE,
    COPY,
    BUNDLE,
    UNKNOWN,
};

class IRHICommandAllocator : public IRHIResource
{
public:
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) = 0;
};
