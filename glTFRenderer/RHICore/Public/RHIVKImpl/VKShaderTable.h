#pragma once
#include "RHIInterface/IRHIShaderTable.h"

class RHICORE_API VKShaderTable : public IRHIShaderTable
{
public:
    virtual bool InitShaderTable(IRHIDevice& device, IRHIMemoryManager& memory_manager, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts) override;
};
