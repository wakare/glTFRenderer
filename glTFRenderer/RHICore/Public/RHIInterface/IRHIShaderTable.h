#pragma once
#include "RHIInterface/IRHIDevice.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIRayTracingAS.h"
#include "RHIInterface/IRHIResource.h"

struct RHICORE_API  RHIShaderTableRecordBase
{
    virtual ~RHIShaderTableRecordBase() = default;

    virtual void* GetData() { return nullptr; }
    virtual size_t GetSize() { return 0; }
};

// One ray type mapping one SBT (means one raygen shader, one miss shader and one hit group symbol)
struct RHICORE_API  RHIShaderBindingTable
{
    std::string raygen_entry;
    std::string miss_entry;
    std::string hit_group_entry;

    std::shared_ptr<RHIShaderTableRecordBase> raygen_record;
    std::shared_ptr<RHIShaderTableRecordBase> miss_record;
    std::vector<std::shared_ptr<RHIShaderTableRecordBase>> hit_group_records;
};

class RHICORE_API IRHIShaderTable
{
public:
    virtual bool InitShaderTable(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts) = 0;
};

