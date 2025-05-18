#pragma once
#include "IRHIRenderTargetManager.h"

class VKRenderTargetManager : public IRHIRenderTargetManager
{
public:
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) override;
     
    virtual std::shared_ptr<IRHITextureDescriptorAllocation> CreateRenderTarget(IRHIDevice& device, IRHIMemoryManager& memory_manager, const
        RHITextureDesc& texture_desc, const RHIRenderTargetDesc& render_target_desc) override;
    virtual std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHIMemoryManager& memory_manager, IRHISwapChain& swapChain, RHITextureClearValue clearValue) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIDescriptorAllocation*>& render_targets) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIDescriptorAllocation*>& render_targets) override;
};
