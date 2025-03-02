#pragma once
#include "IRHICommandQueue.h"
#include "IRHIFactory.h"
#include <Windows.h>

#include "IRHISemaphore.h"

class IRHICommandList;

enum SWAP_CHAIN_MODE
{
    VSYNC = 0,
    MAILBOX = 1,
};
struct RHISwapChainDesc
{
    bool full_screen;
    SWAP_CHAIN_MODE chain_mode;
    HWND hwnd;
};

class IRHISwapChain : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHISwapChain)
    
    unsigned GetWidth() const;
    unsigned GetHeight() const;
    RHIDataFormat GetBackBufferFormat() const;
    
    virtual unsigned GetCurrentBackBufferIndex() = 0;
    virtual unsigned GetBackBufferCount() = 0;

    virtual bool InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, const RHITextureDesc& swap_chain_buffer_desc, const
                               RHISwapChainDesc& swap_chain_desc) = 0;
    virtual bool AcquireNewFrame(IRHIDevice& device) = 0;
    virtual IRHISemaphore& GetAvailableFrameSemaphore() = 0;
    virtual bool Present(IRHICommandQueue& command_queue, IRHICommandList& command_list) = 0;
    virtual bool HostWaitPresentFinished(IRHIDevice& device) = 0;
    
    const RHITextureDesc& GetSwapChainBufferDesc() const;
    
protected:
    SWAP_CHAIN_MODE m_swap_chain_mode;
    RHITextureDesc m_swap_chain_buffer_desc;
};
