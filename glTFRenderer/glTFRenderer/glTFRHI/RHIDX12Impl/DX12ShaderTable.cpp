#include "DX12ShaderTable.h"

#include "DX12Device.h"
#include "DX12PipelineStateObject.h"

bool DX12ShaderTable::InitShaderTable(IRHIDevice& device, IRHIPipelineStateObject& pso, const RayTracingShaderEntryFunctionNames& entry_names)
{
    auto* dx_pso_props = dynamic_cast<DX12DXRStateObject&>(pso).GetDXRStateObjectProperties();
    
    void* ray_gen_shader_identifier;
    void* miss_shader_identifier;
    void* hit_group_shader_identifier;

    auto GetShaderIdentifiers = [&](auto* state_object_properties)
    {
        auto raygen_shader_entry_name = to_wide_string(entry_names.raygen_shader_entry_name);
        auto miss_shader_entry_name = to_wide_string(entry_names.miss_shader_entry_name);
        auto hit_group_shader_entry_name = to_wide_string(entry_names.hit_group_name);
        
        ray_gen_shader_identifier = state_object_properties->GetShaderIdentifier(raygen_shader_entry_name.c_str());
        miss_shader_identifier = state_object_properties->GetShaderIdentifier(miss_shader_entry_name.c_str());
        hit_group_shader_identifier = state_object_properties->GetShaderIdentifier(hit_group_shader_entry_name.c_str());
    };

    // Get shader identifiers.
    UINT shaderIdentifierSize;
    {
        GetShaderIdentifiers(dx_pso_props);
        shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // Ray gen shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        m_rayGenShaderTable = std::make_shared<DX12GPUBuffer>();
        m_rayGenShaderTable->InitGPUBuffer(device, {
                L"RayGenShaderTable",
                static_cast<size_t>(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT),
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer
            });
        m_rayGenShaderTable->UploadBufferFromCPU(ray_gen_shader_identifier, 0, shaderIdentifierSize);
    }

    // Miss shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        m_missShaderTable = std::make_shared<DX12GPUBuffer>();
        m_missShaderTable->InitGPUBuffer(device, {
                L"MissShaderTable",
                static_cast<size_t>(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT),
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer
            });
        m_missShaderTable->UploadBufferFromCPU(miss_shader_identifier, 0, shaderIdentifierSize);
    }

    // Hit group shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        m_hitGroupShaderTable = std::make_shared<DX12GPUBuffer>();
        m_hitGroupShaderTable->InitGPUBuffer(device, {
                L"HitGroupShaderTable",
                static_cast<size_t>(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT),
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer
            });
        m_hitGroupShaderTable->UploadBufferFromCPU(hit_group_shader_identifier, 0, shaderIdentifierSize);
    }
    
    return true;
}

ID3D12Resource* DX12ShaderTable::GetRayGenShaderTable() const
{
    return m_rayGenShaderTable->GetBuffer();
}

ID3D12Resource* DX12ShaderTable::GetMissShaderTable() const
{
    return m_missShaderTable->GetBuffer();
}

ID3D12Resource* DX12ShaderTable::GetHitGroupShaderTable() const
{
    return m_hitGroupShaderTable->GetBuffer();
}
