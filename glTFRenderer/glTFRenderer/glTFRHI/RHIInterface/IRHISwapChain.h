#pragma once
#include "IRHICommandQueue.h"
#include "IRHIFactory.h"
#include "../../glTFWindow/glTFWindow.h"

class IRHISwapChain : public IRHIResource
{
public:
    virtual bool InitSwapChain(IRHIFactory& factory, IRHICommandQueue& commandQueue, unsigned width, unsigned height, bool fullScreen, glTFWindow& window) = 0;
};
