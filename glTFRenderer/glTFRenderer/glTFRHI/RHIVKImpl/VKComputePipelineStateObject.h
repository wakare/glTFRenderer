#pragma once
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKComputePipelineStateObject : public IRHIComputePipelineStateObject
{
public:
    VKComputePipelineStateObject() = default;
    virtual ~VKComputePipelineStateObject() override;

    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::vector<RHIPipelineInputLayout>& input_layouts) override;
};
