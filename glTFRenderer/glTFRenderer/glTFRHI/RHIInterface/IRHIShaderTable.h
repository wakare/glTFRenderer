#pragma once
#include "IRHIDevice.h"
#include "IRHIPipelineStateObject.h"
#include "IRHIResource.h"

class IRHIShaderTable : public IRHIResource
{
public:
    virtual bool InitShaderTable(IRHIDevice& device, IRHIPipelineStateObject& pso) = 0;
};
