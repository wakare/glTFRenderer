#pragma once
#include "glTFRHI/RHIInterface/IRHIRenderPass.h"

class DX12RenderPass : public IRHIRenderPass
{
public:
    virtual bool InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info) override;
    
    virtual bool Release(glTFRenderResourceManager&) override;
};
