#pragma once
#include "IRHICommandQueue.h"
#include "IRHIFactory.h"

class glTFWindow;

class IRHISwapChain : public IRHIResource
{
public:
    virtual unsigned GetWidth() = 0;
    virtual unsigned GetHeight() = 0;
    
    virtual bool InitSwapChain(IRHIFactory& factory, IRHICommandQueue& commandQueue, unsigned width, unsigned height, bool fullScreen, glTFWindow& window) = 0;
};
