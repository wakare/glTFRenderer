#pragma once
#include "glTFRHI/RHIInterface/IRHIShaderTable.h"
#include "DX12Common.h"
#include "DX12GPUBuffer.h"

class DX12ShaderTable : public IRHIShaderTable
{
public:
    virtual bool InitShaderTable(IRHIDevice& device, IRHIPipelineStateObject& pso, const RayTracingShaderEntryFunctionNames& entry_names) override;

    ID3D12Resource* GetRayGenShaderTable() const;
    ID3D12Resource* GetMissShaderTable() const;
    ID3D12Resource* GetHitGroupShaderTable() const;
    
protected:
    std::shared_ptr<DX12GPUBuffer> m_missShaderTable;
    std::shared_ptr<DX12GPUBuffer> m_hitGroupShaderTable;
    std::shared_ptr<DX12GPUBuffer> m_rayGenShaderTable;
};
