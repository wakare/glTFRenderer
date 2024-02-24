#pragma once
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKGraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject
{
public:
    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature) override;
    virtual bool BindSwapChain(const IRHISwapChain& swapchain) override;
    virtual bool BindRenderTargetFormats(const std::vector<IRHIRenderTarget*>& render_targets) override;
};
