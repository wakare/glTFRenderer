#pragma once
#include "glTFRHI/RHIInterface/IRHIShaderTable.h"

class VKShaderTable : public IRHIShaderTable
{
public:
    virtual bool InitShaderTable(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts) override;
};
