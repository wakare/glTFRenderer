#pragma once
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKRTPipelineStateObject : public IRHIRayTracingPipelineStateObject
{
public:
    VKRTPipelineStateObject() = default;
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::vector<RHIPipelineInputLayout>& input_layouts) override;
};
