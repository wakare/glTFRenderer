#pragma once
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

class VKRenderTargetManager : public IRHIRenderTargetManager
{
public:
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) override;
     
    virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, const
                                                                 RHITextureDesc& texture_desc, const RHIRenderTargetDesc& render_target_desc) override;
    virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, RHITextureClearValue clearValue) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIDescriptorAllocation*>& render_targets, const
                                   RHITextureClearValue& render_target_clear_value, const RHITextureClearValue& depth_stencil_clear_value) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets, /*optional*/ IRHIRenderTarget* depthStencil) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIDescriptorAllocation*>& render_targets, /*optional*/ IRHIDescriptorAllocation* depth_stencil) override;
    
};
