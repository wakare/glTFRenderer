#include "DX12ShaderTable.h"

#include "DX12Device.h"
#include "DX12PipelineStateObject.h"

bool DX12ShaderTable::InitShaderTable(IRHIDevice& device, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts)
{
    auto* dx_pso_props = dynamic_cast<DX12DXRStateObject&>(pso).GetDXRStateObjectProperties();
    const auto& hit_group_descs = dynamic_cast<DX12DXRStateObject&>(pso).GetHitGroupDescs();
    
    // Get shader identifiers.
    const UINT shader_identifier_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const size_t ray_count = sbts.size();
    
    // Ray gen shader table
    {
        size_t raygen_buffer_size = ray_count * shader_identifier_size;
        for (const auto& sbt : sbts)
        {
            raygen_buffer_size += sbt.raygen_record ? sbt.raygen_record->GetSize() : 0;
        }

        const std::unique_ptr<char[]> temporary_buffer = std::make_unique<char[]>(raygen_buffer_size);
        char* data = temporary_buffer.get();
        for (const auto& sbt : sbts)
        {
            const auto raygen_shader_entry_name = to_wide_string(sbt.raygen_entry);
            const void* ray_gen_shader_identifier = dx_pso_props->GetShaderIdentifier(raygen_shader_entry_name.c_str());

            memcpy(data, ray_gen_shader_identifier, shader_identifier_size);
            data += shader_identifier_size;
            
            if (sbt.raygen_record)
            {
                memcpy(data, sbt.raygen_record->GetData(), sbt.raygen_record->GetSize());
                data += sbt.raygen_record->GetSize();
            }
        }
        
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
        m_rayGenShaderTable->UploadBufferFromCPU(temporary_buffer.get(), 0, raygen_buffer_size);
    }

    // Miss shader table
    {
        size_t miss_buffer_size = ray_count * shader_identifier_size;
        for (const auto& sbt : sbts)
        {
            miss_buffer_size += sbt.miss_record ? sbt.miss_record->GetSize() : 0;
        }

        const std::unique_ptr<char[]> temporary_buffer = std::make_unique<char[]>(miss_buffer_size);
        char* data = temporary_buffer.get();
        for (const auto& sbt : sbts)
        {
            const auto miss_shader_entry_name = to_wide_string(sbt.miss_entry);
            const void* miss_shader_identifier = dx_pso_props->GetShaderIdentifier(miss_shader_entry_name.c_str());

            memcpy(data, miss_shader_identifier, shader_identifier_size);
            data += shader_identifier_size;
            
            if (sbt.miss_record)
            {
                memcpy(data, sbt.miss_record->GetData(), sbt.miss_record->GetSize());
                data += sbt.miss_record->GetSize();
            }
        }

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
        m_missShaderTable->UploadBufferFromCPU(temporary_buffer.get(), 0, miss_buffer_size);
    }

    // Hit group shader table
    {
        // Build hit group shader record layout
        
        const auto hit_group_shader_entry_name = to_wide_string(sbts.front().hit_group_entry);
        void* hit_group_shader_identifier = dx_pso_props->GetShaderIdentifier(hit_group_shader_entry_name.c_str());   
        
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shader_identifier_size;
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
        m_hitGroupShaderTable->UploadBufferFromCPU(hit_group_shader_identifier, 0, shader_identifier_size);
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
