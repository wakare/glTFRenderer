#pragma once
#include "../RHIInterface/IRHISwapChain.h"
#include <dxgi1_4.h>

class DX12SwapChain : public IRHISwapChain
{
public:
    DX12SwapChain();
    
    virtual bool InitSwapChain(IRHIFactory& factory,IRHICommandQueue& commandQueue, unsigned width, unsigned height, bool fullScreen, glTFWindow& window) override;

    IDXGISwapChain3* GetSwapChain() {return m_swapChain;}
    const IDXGISwapChain3* GetSwapChain() const {return m_swapChain;}

    DXGI_SAMPLE_DESC& GetSwapChainSampleDesc() {return m_swapChainSampleDesc;}
    const DXGI_SAMPLE_DESC& GetSwapChainSampleDesc() const {return m_swapChainSampleDesc;}

    unsigned GetFrameBufferCount() const {return m_frameBufferCount;}
    unsigned GetCurrentFrameBufferIndex() const {return m_swapChain->GetCurrentBackBufferIndex();}
    
private:
    unsigned m_frameBufferCount;
    
    IDXGISwapChain3* m_swapChain;
    DXGI_SAMPLE_DESC m_swapChainSampleDesc;
};

