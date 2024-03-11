#pragma once
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKComputePipelineStateObject : public IRHIComputePipelineStateObject
{
public:
    VKComputePipelineStateObject() = default;
    virtual ~VKComputePipelineStateObject() override;

    virtual bool InitPipelineStateObject(IRHIDevice& device, const RHIPipelineStateInfo& pipeline_state_info) override;
};
