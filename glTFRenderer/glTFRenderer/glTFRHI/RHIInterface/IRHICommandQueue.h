#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHICommandQueue : public IRHIResource
{
public:
    virtual bool InitCommandQueue(IRHIDevice& device) = 0;
};
 