#pragma once
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKRTPipelineStateObject : public IRHIRayTracingPipelineStateObject
{
public:
    VKRTPipelineStateObject() = default;
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain) override;

    virtual bool Release(glTFRenderResourceManager&) override;
    
};
