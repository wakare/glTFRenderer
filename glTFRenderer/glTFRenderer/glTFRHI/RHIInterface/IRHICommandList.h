#pragma once
#include "IRHICommandAllocator.h"
#include "IRHIResource.h"

class IRHICommandList : public IRHIResource
{
public:
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& command_allocator) = 0;
};
