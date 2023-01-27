#pragma once
#include "IRHICommandQueue.h"
#include "IRHIResource.h"

class IRHIFence : public IRHIResource
{
public:
    virtual bool InitFence(IRHIDevice& device) = 0;
    virtual bool SignalWhenCommandQueueFinish(IRHICommandQueue& commandQueue) = 0;
    virtual bool WaitUtilSignal() = 0;
};
