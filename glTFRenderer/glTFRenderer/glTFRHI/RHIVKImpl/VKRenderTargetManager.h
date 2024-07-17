#pragma once
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

class VKRenderTargetManager : public IRHIRenderTargetManager
{
public:
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) override;
     
    virtual std::shared_ptr<IRHITextureDescriptorAllocation> CreateRenderTarget(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const
                                                                 RHITextureDesc& texture_desc, const RHIRenderTargetDesc& render_target_desc) override;
    virtual std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> CreateRenderTargetFromSwapChain(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHISwapChain& swapChain, RHITextureClearValue clearValue) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIDescriptorAllocation*>& render_targets) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIDescriptorAllocation*>& render_targets, /*optional*/ IRHIDescriptorAllocation* depth_stencil) override;
};
