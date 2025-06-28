#pragma once
#include "RHIInterface/IRHIRenderPass.h"
#include "RHIInterface/IRHIRenderTarget.h"
#include "RHIInterface/IRHIResource.h"
#include "RHIInterface/IRHISwapChain.h"

struct RHIFrameBufferInfo
{
    std::shared_ptr<IRHIRenderPass> render_pass;
    std::vector<std::shared_ptr<IRHIRenderTarget>> attachment_images;
};

class RHICORE_API IRHIFrameBuffer : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIFrameBuffer)
    
    virtual bool InitFrameBuffer(IRHIDevice& device, IRHISwapChain& swap_chain, const RHIFrameBufferInfo& info) = 0;
};
