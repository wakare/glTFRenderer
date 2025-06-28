#pragma once
#include "RHIInterface/IRHIDevice.h"
#include "RHIInterface/IRHIResource.h"

class RHICORE_API IRHICommandQueue : public IRHIResource
{
public:
    virtual bool InitCommandQueue(IRHIDevice& device) = 0;
};
 