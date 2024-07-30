#include "VKComputePipelineStateObject.h"

VKComputePipelineStateObject::~VKComputePipelineStateObject()
{
    
}

bool VKComputePipelineStateObject::InitPipelineStateObject(IRHIDevice& device,
    const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::vector<RHIPipelineInputLayout>& input_layouts)
{
    return true;
}
