#pragma once
#include <d3d12.h>
#include <memory>

#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

class DX12RenderTargetManager : public IRHIRenderTargetManager
{
public:
    DX12RenderTargetManager();
    virtual ~DX12RenderTargetManager() override;
    
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count) override;
    virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat resource_format, RHIDataFormat descriptor_format, const
                                                                 RHITextureDesc& desc) override;
    virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, RHITextureClearValue clearValue) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& render_targets) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& render_targets, const
                                   RHITextureClearValue& render_target_clear_value, const RHITextureClearValue& depth_stencil_clear_value) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets, /*optional*/ IRHIRenderTarget* depthStencil) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& render_targets, /*optional*/ IRHIDescriptorAllocation* depth_stencil) override;
    
private:
    std::shared_ptr<IRHIRenderTarget> CreateRenderTargetWithResource(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat descriptor_format,
                                                                     std::shared_ptr<IRHITextureAllocation> texture, const D3D12_CLEAR_VALUE& clear_value);
   
    std::vector<std::shared_ptr<IRHITextureAllocation>> m_external_textures;
};
