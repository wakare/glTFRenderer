#include "VKPipelineStateObject.h"

bool VKGraphicsPipelineStateObject::InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature)
{
    return true;
}

bool VKGraphicsPipelineStateObject::BindSwapChain(const IRHISwapChain& swapchain)
{
    return true;
}

bool VKGraphicsPipelineStateObject::BindRenderTargetFormats(const std::vector<IRHIRenderTarget*>& render_targets)
{
    return true;
}
