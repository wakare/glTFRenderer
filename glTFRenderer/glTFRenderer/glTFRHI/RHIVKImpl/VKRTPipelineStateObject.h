#pragma once
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class VKRTPipelineStateObject : public IRHIRayTracingPipelineStateObject
{
public:
    VKRTPipelineStateObject() = default;
    virtual bool InitPipelineStateObject(IRHIDevice& device, const RHIPipelineStateInfo& pipeline_state_info) override;
};
