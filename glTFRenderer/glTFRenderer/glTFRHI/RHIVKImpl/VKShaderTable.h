#pragma once
#include "glTFRHI/RHIInterface/IRHIShaderTable.h"

class VKShaderTable : public IRHIShaderTable
{
public:
    virtual bool InitShaderTable(IRHIDevice& device, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts) override;
};
