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
    
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count) override;
    virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat resource_format, RHIDataFormat descriptor_format, const IRHIRenderTargetDesc& desc) override;
    virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, RHIRenderTargetClearValue clearValue) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets, /*optional*/ IRHIRenderTarget* depthStencil) override;
    
private:
    std::shared_ptr<IRHIRenderTarget> CreateRenderTargetWithResource(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat descriptor_format,
        ID3D12Resource* texture_resource, const D3D12_CLEAR_VALUE& clear_value);
    
    size_t                      m_max_render_target_count;

    std::shared_ptr<IRHIDescriptorHeap> m_rtv_descriptor_heap;
    std::shared_ptr<IRHIDescriptorHeap> m_dsv_descriptor_heap;
     
    // key - renderTarget id, value - resource descriptor handle
    // each rt created within rt manager should be record in this map
    std::map<RTID, D3D12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;
    std::map<RTID, D3D12_CPU_DESCRIPTOR_HANDLE> m_dsvHandles;
};
