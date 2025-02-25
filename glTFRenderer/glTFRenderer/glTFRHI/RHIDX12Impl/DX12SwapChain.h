#pragma once
#include "../RHIInterface/IRHISwapChain.h"
#include "DX12Common.h"
#include <dxgi1_4.h>

#include "DX12Fence.h"

class DX12SwapChain : public IRHISwapChain
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12SwapChain)
    
    virtual unsigned GetCurrentBackBufferIndex() override;
    virtual unsigned GetBackBufferCount() override;
    
    virtual bool InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& command_queue, const RHITextureDesc& swap_chain_buffer_desc, const
                               RHISwapChainDesc& swap_chain_desc) override;
    virtual bool AcquireNewFrame(IRHIDevice& device) override;
    virtual IRHISemaphore& GetAvailableFrameSemaphore() override;
    virtual bool Present(IRHICommandQueue& command_queue, IRHICommandList& command_list) override;
    virtual bool HostWaitPresentFinished(IRHIDevice& device) override;
    virtual bool Release(glTFRenderResourceManager&) override;
    
    IDXGISwapChain3* GetSwapChain() {return m_swap_chain.Get();}
    const IDXGISwapChain3* GetSwapChain() const {return m_swap_chain.Get();}

    DXGI_SAMPLE_DESC& GetSwapChainSampleDesc() {return m_swap_chain_sample_desc;}
    const DXGI_SAMPLE_DESC& GetSwapChainSampleDesc() const {return m_swap_chain_sample_desc;}

    unsigned GetFrameBufferCount() const {return m_frame_buffer_count;}
    unsigned GetCurrentFrameBufferIndex() const {return m_swap_chain->GetCurrentBackBufferIndex();}
    
private:
    std::shared_ptr<IRHIFence> m_fence;
    
    unsigned m_frame_buffer_count {3};
    ComPtr<IDXGISwapChain3> m_swap_chain {nullptr};
    DXGI_SAMPLE_DESC m_swap_chain_sample_desc {};
};

