#pragma once
#include "glTFRHI/RHIInterface/IRHIRenderPass.h"

class Dx12RenderPass : public IRHIRenderPass
{
public:
    virtual bool InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info) override;
};
