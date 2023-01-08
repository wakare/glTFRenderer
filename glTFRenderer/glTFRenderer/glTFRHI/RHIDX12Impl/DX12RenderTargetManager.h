#pragma once
#include <d3d12.h>
#include <map>
#include <memory>

#include "../RHIInterface/IRHIRenderTargetManager.h"

class DX12RenderTargetManager : public IRHIRenderTargetManager
{
public:
    DX12RenderTargetManager();
    
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) override;
    virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIRenderTargetFormat format, const IRHIRenderTargetDesc& desc) override;
    virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, IRHIRenderTargetClearValue clearValue) override;
    
private:
    static DXGI_FORMAT ConvertToDXFormat(RHIRenderTargetFormat format);
    
    size_t                      m_maxRenderTargetCount;
    
    ID3D12DescriptorHeap*       m_rtvDescriptorHeap;
    ID3D12DescriptorHeap*       m_dsvDescriptorHeap;

    // key - renderTarget id, value - resource descriptor handle
    // each rt created within rt manager should be record in this map
    std::map<RTID, D3D12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;
    std::map<RTID, D3D12_CPU_DESCRIPTOR_HANDLE> m_dsvHandles;
};
