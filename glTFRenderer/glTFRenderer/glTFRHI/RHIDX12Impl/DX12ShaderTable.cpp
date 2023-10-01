#include "DX12ShaderTable.h"

#include "DX12Device.h"
#include "DX12PipelineStateObject.h"

bool DX12ShaderTable::InitShaderTable(IRHIDevice& device, IRHIPipelineStateObject& pso)
{
    auto* dxPSOProps = dynamic_cast<DX12DXRStateObject&>(pso).GetDXRStateObjectProperties();
    
    void* rayGenShaderIdentifier;
    void* missShaderIdentifier;
    void* hitGroupShaderIdentifier;

    auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
    {
        rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(L"MyRaygenShader");
        missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(L"MyMissShader");
        hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(L"MyHitGroup");
    };

    // Get shader identifiers.
    UINT shaderIdentifierSize;
    {
        GetShaderIdentifiers(dxPSOProps);
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
        m_rayGenShaderTable->UploadBufferFromCPU(rayGenShaderIdentifier, 0, shaderIdentifierSize);
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
        m_missShaderTable->UploadBufferFromCPU(missShaderIdentifier, 0, shaderIdentifierSize);
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
        m_hitGroupShaderTable->UploadBufferFromCPU(hitGroupShaderIdentifier, 0, shaderIdentifierSize);
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
