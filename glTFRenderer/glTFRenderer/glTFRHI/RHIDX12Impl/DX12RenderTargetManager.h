#pragma once
#include <d3d12.h>
#include <map>
#include <memory>

#include "../RHIInterface/IRHIDescriptorHeap.h"
#include "../RHIInterface/IRHIRenderTargetManager.h"

class DX12RenderTargetManager : public IRHIRenderTargetManager
{
public:
    DX12RenderTargetManager();
    virtual ~DX12RenderTargetManager() override;
    
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) override;
    virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat format, const IRHIRenderTargetDesc& desc) override;
    virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, RHIRenderTargetClearValue clearValue) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, IRHIRenderTarget* renderTargetArray, size_t renderTargetArrayCount) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, IRHIRenderTarget* renderTargetArray, size_t renderTargetArraySize, /*optional*/ IRHIRenderTarget* depthStencil ) override;
    
private:    
    size_t                      m_maxRenderTargetCount;

    std::shared_ptr<IRHIDescriptorHeap> m_rtvDescriptorHeap;
    std::shared_ptr<IRHIDescriptorHeap> m_dsvDescriptorHeap;
    
    //ID3D12DescriptorHeap*       m_rtvDescriptorHeap;
    //ID3D12DescriptorHeap*       m_dsvDescriptorHeap;

    // key - renderTarget id, value - resource descriptor handle
    // each rt created within rt manager should be record in this map
    std::map<RTID, D3D12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;
    std::map<RTID, D3D12_CPU_DESCRIPTOR_HANDLE> m_dsvHandles;
};
