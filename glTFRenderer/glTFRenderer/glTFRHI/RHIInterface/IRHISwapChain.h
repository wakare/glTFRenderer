#pragma once
#include "IRHICommandQueue.h"
#include "IRHIFactory.h"
#include <Windows.h>

#include "IRHISemaphore.h"

class IRHISwapChain : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHISwapChain)
    
    virtual unsigned GetWidth() = 0;
    virtual unsigned GetHeight() = 0;
    RHIDataFormat GetBackBufferFormat() const;
    
    virtual unsigned GetCurrentBackBufferIndex() = 0;
    virtual unsigned GetBackBufferCount() = 0;

    virtual bool InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, unsigned width, unsigned height, bool fullScreen, HWND hwnd) = 0;
    virtual bool AcquireNewFrame(IRHIDevice& device) = 0;
    virtual IRHISemaphore& GetAvailableFrameSemaphore() = 0;
    virtual bool Present(IRHICommandQueue& command_queue) = 0;

protected:
    RHIDataFormat m_back_buffer_format {RHIDataFormat::R8G8B8A8_UNORM};
};
