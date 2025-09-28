#pragma once
#include "DX12Common.h"
#include "RHIInterface/IRHIShaderTable.h"
#include "RHIInterface/IRHIMemoryManager.h"

class RHICORE_API DX12ShaderTable : public IRHIShaderTable
{
public:
    DX12ShaderTable();
    virtual bool InitShaderTable(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts) override;

    ID3D12Resource* GetRayGenShaderTable() const;
    ID3D12Resource* GetMissShaderTable() const;
    ID3D12Resource* GetHitGroupShaderTable() const;

    size_t GetRayGenStride() const {return m_raygen_shader_table_stride; }
    size_t GetMissStride() const {return m_miss_shader_table_stride; }
    size_t GetHitGroupStride() const {return m_hit_group_shader_table_stride; }
    
protected:
    std::shared_ptr<IRHIBufferAllocation> m_missShaderTable;
    std::shared_ptr<IRHIBufferAllocation> m_hitGroupShaderTable;
    std::shared_ptr<IRHIBufferAllocation> m_rayGenShaderTable;

    size_t m_raygen_shader_table_stride;
    size_t m_miss_shader_table_stride;
    size_t m_hit_group_shader_table_stride;
};
