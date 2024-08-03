#pragma once
#include "../RHIInterface/IRHISwapChain.h"
#include "DX12Common.h"
#include <dxgi1_4.h>

class DX12SwapChain : public IRHISwapChain
{
public:
    DX12SwapChain();
    virtual ~DX12SwapChain() override;

    virtual unsigned GetCurrentBackBufferIndex() override;
    virtual unsigned GetBackBufferCount() override;
    
    virtual bool InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, const RHITextureDesc& swap_chain_buffer_desc, bool fullScreen, HWND hwnd) override;
    virtual bool AcquireNewFrame(IRHIDevice& device) override;
    virtual IRHISemaphore& GetAvailableFrameSemaphore() override;
    virtual bool Present(IRHICommandQueue& command_queue, IRHICommandList& command_list) override;
    virtual bool HostWaitPresentFinished(IRHIDevice& device) override;
    
    IDXGISwapChain3* GetSwapChain() {return m_swap_chain.Get();}
    const IDXGISwapChain3* GetSwapChain() const {return m_swap_chain.Get();}

    DXGI_SAMPLE_DESC& GetSwapChainSampleDesc() {return m_swap_chain_sample_desc;}
    const DXGI_SAMPLE_DESC& GetSwapChainSampleDesc() const {return m_swap_chain_sample_desc;}

    unsigned GetFrameBufferCount() const {return m_frame_buffer_count;}
    unsigned GetCurrentFrameBufferIndex() const {return m_swap_chain->GetCurrentBackBufferIndex();}
    
private:
    unsigned m_frame_buffer_count;
    ComPtr<IDXGISwapChain3> m_swap_chain;
    DXGI_SAMPLE_DESC m_swap_chain_sample_desc;
};

