#include "DX12ShaderTable.h"

#include "DX12Buffer.h"
#include "DX12Device.h"
#include "DX12PipelineStateObject.h"
#include "DX12RayTracingAS.h"

#include <algorithm>
#include <vector>

namespace
{
    size_t AlignUp(size_t value, size_t alignment)
    {
        if (alignment == 0)
        {
            return value;
        }

        return (value + alignment - 1) & ~(alignment - 1);
    }
}

DX12ShaderTable::DX12ShaderTable()
    : m_raygen_shader_table_stride(0)
    , m_miss_shader_table_stride(0)
    , m_hit_group_shader_table_stride(0)
{
}

bool DX12ShaderTable::InitShaderTable(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts)
{
    GLTF_CHECK(!sbts.empty());
    auto* dx_pso_props = dynamic_cast<DX12RTPipelineStateObject&>(pso).GetDXRStateObjectProperties();
    // Get shader identifiers.
    const UINT shader_identifier_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const size_t shader_record_alignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    const size_t ray_count = sbts.size();

    auto allocate_shader_table = [&](const wchar_t* name,
                                     size_t buffer_size,
                                     std::shared_ptr<IRHIBufferAllocation>& out_allocation)
    {
        RHIBufferDesc buffer_desc
        {
            name,
            buffer_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            RUF_RAY_TRACING,
        };
        memory_manager.AllocateBufferMemory(device, buffer_desc, out_allocation);
    };
    
    // Ray gen shader table
    {
        size_t raygen_record_size = 0;
        for (const auto& sbt : sbts)
        {
            const size_t sbt_record_size = sbt.raygen_record ? sbt.raygen_record->GetSize() : 0;
            raygen_record_size = (std::max)(raygen_record_size, sbt_record_size);
        }

        m_raygen_shader_table_stride = AlignUp(shader_identifier_size + raygen_record_size, shader_record_alignment);
        const size_t raygen_buffer_size = m_raygen_shader_table_stride;

        std::vector<unsigned char> temporary_buffer(raygen_buffer_size, 0u);
        unsigned char* data = temporary_buffer.data();
        const auto& raygen_sbt = sbts.front();
        {
            const auto raygen_shader_entry_name = to_wide_string(raygen_sbt.raygen_entry);
            const void* ray_gen_shader_identifier = dx_pso_props->GetShaderIdentifier(raygen_shader_entry_name.c_str());
            GLTF_CHECK(ray_gen_shader_identifier != nullptr);
            memcpy(data, ray_gen_shader_identifier, shader_identifier_size);
            data += shader_identifier_size;
            
            if (raygen_sbt.raygen_record)
            {
                memcpy(data, raygen_sbt.raygen_record->GetData(), raygen_sbt.raygen_record->GetSize());
            }
            data += (m_raygen_shader_table_stride - shader_identifier_size);
        }

        allocate_shader_table(L"RayGenShaderTable", raygen_buffer_size, m_rayGenShaderTable);
        memory_manager.UploadBufferData(device, command_list, *m_rayGenShaderTable, temporary_buffer.data(), 0, raygen_buffer_size);
    }

    // Miss shader table
    {
        size_t miss_record_size = 0;
        for (const auto& sbt : sbts)
        {
            const size_t sbt_record_size = sbt.miss_record ? sbt.miss_record->GetSize() : 0;
            miss_record_size = (std::max)(miss_record_size, sbt_record_size);
        }

        m_miss_shader_table_stride = AlignUp(shader_identifier_size + miss_record_size, shader_record_alignment);
        const size_t miss_buffer_size = ray_count * m_miss_shader_table_stride;

        std::vector<unsigned char> temporary_buffer(miss_buffer_size, 0u);
        unsigned char* data = temporary_buffer.data();
        for (const auto& sbt : sbts)
        {
            const auto miss_shader_entry_name = to_wide_string(sbt.miss_entry);
            const void* miss_shader_identifier = dx_pso_props->GetShaderIdentifier(miss_shader_entry_name.c_str());
            GLTF_CHECK(miss_shader_identifier != nullptr);
            memcpy(data, miss_shader_identifier, shader_identifier_size);
            data += shader_identifier_size;
            
            if (sbt.miss_record)
            {
                memcpy(data, sbt.miss_record->GetData(), sbt.miss_record->GetSize());
            }
            data += (m_miss_shader_table_stride - shader_identifier_size);
        }

        allocate_shader_table(L"MissShaderTable", miss_buffer_size, m_missShaderTable);
        memory_manager.UploadBufferData(device, command_list, *m_missShaderTable, temporary_buffer.data(), 0, miss_buffer_size);
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
                const size_t sbt_record_size = hit_group_record ? hit_group_record->GetSize() : 0;
                hit_group_record_size = (std::max)(hit_group_record_size, sbt_record_size);
            }
        }

        m_hit_group_shader_table_stride = AlignUp(shader_identifier_size + hit_group_record_size, shader_record_alignment);
        const size_t hit_group_record_count = hit_group_instance_count * ray_count;
        const size_t hit_group_buffer_size = hit_group_record_count * m_hit_group_shader_table_stride;
        std::vector<unsigned char> temporary_buffer(hit_group_buffer_size, 0u);
        unsigned char* data = temporary_buffer.data();

        for (const auto& sbt : sbts)
        {
            for (size_t i = 0; i < hit_group_instance_count; ++i)
            {
                const auto hit_group_shader_entry_name = to_wide_string(sbt.hit_group_entry);
                const void* hit_group_shader_identifier = dx_pso_props->GetShaderIdentifier(hit_group_shader_entry_name.c_str());
                GLTF_CHECK(hit_group_shader_identifier != nullptr);
                memcpy(data, hit_group_shader_identifier, shader_identifier_size);
                data += shader_identifier_size;

                if (sbt.hit_group_records.size() > i && sbt.hit_group_records[i])
                {
                    memcpy(data, sbt.hit_group_records[i]->GetData(), sbt.hit_group_records[i]->GetSize());
                }
                data += (m_hit_group_shader_table_stride - shader_identifier_size);
            }
        }

        allocate_shader_table(L"HitGroupShaderTable", hit_group_buffer_size, m_hitGroupShaderTable);
        memory_manager.UploadBufferData(device, command_list, *m_hitGroupShaderTable, temporary_buffer.data(), 0, hit_group_buffer_size);
    }
    
    return true;
}

ID3D12Resource* DX12ShaderTable::GetRayGenShaderTable() const
{
    return dynamic_cast<const DX12Buffer&>(*m_rayGenShaderTable->m_buffer).GetRawBuffer();
}

ID3D12Resource* DX12ShaderTable::GetMissShaderTable() const
{
    return dynamic_cast<const DX12Buffer&>(*m_missShaderTable->m_buffer).GetRawBuffer();
}

ID3D12Resource* DX12ShaderTable::GetHitGroupShaderTable() const
{
    return dynamic_cast<const DX12Buffer&>(*m_hitGroupShaderTable->m_buffer).GetRawBuffer();
}
