#pragma once
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

class VKRenderTargetManager : public IRHIRenderTargetManager
{
public:
    virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) override;
     
    virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat resourceFormat, RHIDataFormat descriptorFormat, const
                                                                 RHITextureDesc& desc) override;
    virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, RHITextureClearValue clearValue) override;
    virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets) override;
    virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets, /*optional*/ IRHIRenderTarget* depthStencil) override;
    
};
