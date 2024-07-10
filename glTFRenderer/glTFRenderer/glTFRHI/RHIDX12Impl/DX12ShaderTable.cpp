#include "DX12ShaderTable.h"

#include "DX12Buffer.h"
#include "DX12Device.h"
#include "DX12PipelineStateObject.h"
#include "DX12RayTracingAS.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"

DX12ShaderTable::DX12ShaderTable()
    : m_raygen_shader_table_stride(0)
    , m_miss_shader_table_stride(0)
    , m_hit_group_shader_table_stride(0)
{
}

bool DX12ShaderTable::InitShaderTable(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts)
{
    auto* dx_pso_props = dynamic_cast<DX12RTPipelineStateObject&>(pso).GetDXRStateObjectProperties();
    const auto& hit_group_descs = dynamic_cast<DX12RTPipelineStateObject&>(pso).GetHitGroupDescs();
    
    // Get shader identifiers.
    const UINT shader_identifier_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const size_t ray_count = sbts.size();
    
    // Ray gen shader table
    {
        size_t raygen_record_size = 0;
        for (const auto& sbt : sbts)
        {
            size_t sbt_record_size = sbt.raygen_record ? sbt.raygen_record->GetSize() : 0; 
            raygen_record_size = (sbt_record_size > raygen_record_size) ? sbt_record_size : raygen_record_size;
        }
        m_raygen_shader_table_stride = raygen_record_size + shader_identifier_size;
        
        size_t raygen_buffer_size = ray_count * (shader_identifier_size + raygen_record_size);
        
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
            }
            data += raygen_record_size;
        }

        resource_manager.GetMemoryManager().AllocateBufferMemory(
        device, {
            L"RayGenShaderTable",
            raygen_buffer_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
        },
        m_rayGenShaderTable);
        resource_manager.GetMemoryManager().UploadBufferData(*m_rayGenShaderTable, temporary_buffer.get(), 0, raygen_buffer_size);
    }

    // Miss shader table
    {
        size_t miss_record_size = 0;
        for (const auto& sbt : sbts)
        {
            size_t sbt_record_size = sbt.miss_record ? sbt.miss_record->GetSize() : 0; 
            miss_record_size = (sbt_record_size > miss_record_size) ? sbt_record_size : miss_record_size;
        }
        m_miss_shader_table_stride = miss_record_size + shader_identifier_size;
        
        const size_t miss_buffer_size = ray_count * (shader_identifier_size + miss_record_size);

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
            }
            data += miss_record_size;
        }
        resource_manager.GetMemoryManager().AllocateBufferMemory(
        device, {
            L"MissShaderTable",
            miss_buffer_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COMMON,
        },
        m_missShaderTable);
        resource_manager.GetMemoryManager().UploadBufferData(*m_missShaderTable, temporary_buffer.get(), 0, miss_buffer_size);
    }

    // Hit group shader table
    {
        // Build hit group shader record layout
        const auto& instance_descs = dynamic_cast<DX12RayTracingAS&>(as).GetInstanceDesc();
        const size_t hit_group_instance_count = instance_descs.size();
        
        size_t hit_group_record_size = 0;
        for (const auto& sbt : sbts)
        {
            for (const auto& hit_group_record : sbt.hit_group_records)
            {
                size_t sbt_record_size = hit_group_record ? hit_group_record->GetSize() : 0; 
                hit_group_record_size = (sbt_record_size > hit_group_record_size) ? sbt_record_size : hit_group_record_size;    
            }
        }
        m_hit_group_shader_table_stride = hit_group_record_size + shader_identifier_size;

        const size_t hit_group_buffer_size = hit_group_instance_count * ray_count * (shader_identifier_size + hit_group_record_size);
        const std::unique_ptr<char[]> temporary_buffer = std::make_unique<char[]>(hit_group_buffer_size);
        char* data = temporary_buffer.get();

        for (const auto& sbt : sbts)
        {
            for (size_t i = 0; i < hit_group_instance_count; ++i)
            {
                //GLTF_CHECK(sbt.hit_group_records.size() == hit_group_instance_count);
                
                const auto hit_group_shader_entry_name = to_wide_string(sbts.front().hit_group_entry);
                const void* hit_group_shader_identifier = dx_pso_props->GetShaderIdentifier(hit_group_shader_entry_name.c_str());

                memcpy(data, hit_group_shader_identifier, shader_identifier_size);
                data += shader_identifier_size;

                if (sbt.hit_group_records.size() > i && sbt.hit_group_records[i])
                {
                    memcpy(data, sbt.hit_group_records[i]->GetData(), sbt.hit_group_records[i]->GetSize());
                }
                data += hit_group_record_size;
            }
        }
        resource_manager.GetMemoryManager().AllocateBufferMemory(
        device, {
            L"HitGroupShaderTable",
            hit_group_buffer_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
        },
        m_hitGroupShaderTable);
        resource_manager.GetMemoryManager().UploadBufferData(*m_hitGroupShaderTable, temporary_buffer.get(), 0, hit_group_buffer_size);
    }
    
    return true;
}

ID3D12Resource* DX12ShaderTable::GetRayGenShaderTable() const
{
    return dynamic_cast<const DX12Buffer&>(*m_rayGenShaderTable->m_buffer).GetBuffer();
}

ID3D12Resource* DX12ShaderTable::GetMissShaderTable() const
{
    return dynamic_cast<const DX12Buffer&>(*m_missShaderTable->m_buffer).GetBuffer();
}

ID3D12Resource* DX12ShaderTable::GetHitGroupShaderTable() const
{
    return dynamic_cast<const DX12Buffer&>(*m_hitGroupShaderTable->m_buffer).GetBuffer();
}