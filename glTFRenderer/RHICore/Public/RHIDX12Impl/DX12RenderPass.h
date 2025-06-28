#pragma once
#include "RHIInterface/IRHIRenderPass.h"

class DX12RenderPass : public IRHIRenderPass
{
public:
    virtual bool InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info) override;
    
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
};
