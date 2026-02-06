#include "VKShaderTable.h"

#include "RendererCommon.h"
#include "VKBuffer.h"
#include "VKDevice.h"
#include "VKRTPipelineStateObject.h"
#include "RHIInterface/IRHIMemoryManager.h"

#include <algorithm>

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

    VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer)
    {
        VkBufferDeviceAddressInfo address_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        address_info.buffer = buffer;
        return vkGetBufferDeviceAddress(device, &address_info);
    }
}

bool VKShaderTable::InitShaderTable(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager& memory_manager,
                                    IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts)
{
    (void)as;
    
    GLTF_CHECK(!sbts.empty());
    auto& vk_device = dynamic_cast<VKDevice&>(device);
    const VkDevice raw_device = vk_device.GetDevice();
    const auto& rt_props = vk_device.GetRayTracingPipelineProperties();
    const auto& vk_pso = dynamic_cast<VKRTPipelineStateObject&>(pso);

    const uint32_t handle_size = rt_props.shaderGroupHandleSize;
    const uint32_t handle_alignment = rt_props.shaderGroupHandleAlignment;
    const uint32_t base_alignment = rt_props.shaderGroupBaseAlignment;
    const size_t ray_count = sbts.size();

    const uint32_t group_count = vk_pso.GetShaderGroupCount();
    std::vector<uint8_t> group_handles(group_count * handle_size);
    GLTF_CHECK(vkGetRayTracingShaderGroupHandlesKHR(raw_device, vk_pso.GetPipeline(), 0, group_count, group_handles.size(), group_handles.data()) == VK_SUCCESS);

    auto get_group_handle = [&](uint32_t group_index) -> const uint8_t*
    {
        return group_handles.data() + (group_index * handle_size);
    };

    auto build_table = [&](const wchar_t* name, size_t record_size, size_t record_count, auto&& write_record, std::shared_ptr<IRHIBufferAllocation>& out_allocation,
        VkStridedDeviceAddressRegionKHR& out_region)
    {
        const size_t aligned_record_size = AlignUp(record_size, handle_alignment);
        const size_t stride = AlignUp(aligned_record_size, base_alignment);
        const size_t buffer_size = stride * record_count;

        std::vector<uint8_t> buffer_data(buffer_size, 0);
        uint8_t* dst = buffer_data.data();
        for (size_t i = 0; i < record_count; ++i)
        {
            write_record(dst, i, stride);
            dst += stride;
        }

        RHIBufferDesc buffer_desc
        {
            name,
            buffer_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_RAY_TRACING),
        };

        memory_manager.AllocateBufferMemory(device, buffer_desc, out_allocation);
        memory_manager.UploadBufferData(device, command_list, *out_allocation, buffer_data.data(), 0, buffer_size);

        VkBuffer vk_buffer = dynamic_cast<VKBuffer&>(*out_allocation->m_buffer).GetRawBuffer();
        out_region.deviceAddress = GetBufferDeviceAddress(raw_device, vk_buffer);
        out_region.stride = stride;
        out_region.size = buffer_size;
    };

    // RayGen shader table (must have size == stride)
    {
        size_t raygen_record_size = 0;
        for (const auto& sbt : sbts)
        {
            if (sbt.raygen_record)
            {
                raygen_record_size = std::max(raygen_record_size, sbt.raygen_record->GetSize());
            }
        }

        build_table(
            L"RayGenShaderTable",
            handle_size + raygen_record_size,
            1,
            [&](uint8_t* dst, size_t index, size_t stride)
            {
                const auto& sbt = sbts[0];
                const uint32_t group_index = vk_pso.GetShaderGroupIndex(sbt.raygen_entry);
                memcpy(dst, get_group_handle(group_index), handle_size);
                if (sbt.raygen_record)
                {
                    memcpy(dst + handle_size, sbt.raygen_record->GetData(), sbt.raygen_record->GetSize());
                }
                (void)index;
                (void)stride;
            },
            m_raygen_shader_table,
            m_raygen_region);
    }

    // Miss shader table
    {
        size_t miss_record_size = 0;
        for (const auto& sbt : sbts)
        {
            if (sbt.miss_record)
            {
                miss_record_size = std::max(miss_record_size, sbt.miss_record->GetSize());
            }
        }

        build_table(
            L"MissShaderTable",
            handle_size + miss_record_size,
            ray_count,
            [&](uint8_t* dst, size_t index, size_t stride)
            {
                const auto& sbt = sbts[index];
                const uint32_t group_index = vk_pso.GetShaderGroupIndex(sbt.miss_entry);
                memcpy(dst, get_group_handle(group_index), handle_size);
                if (sbt.miss_record)
                {
                    memcpy(dst + handle_size, sbt.miss_record->GetData(), sbt.miss_record->GetSize());
                }
                (void)stride;
            },
            m_miss_shader_table,
            m_miss_region);
    }

    // Hit group shader table
    {
        size_t hit_group_record_size = 0;
        size_t hit_group_record_count = 0;
        for (const auto& sbt : sbts)
        {
            hit_group_record_count = std::max(hit_group_record_count, sbt.hit_group_records.size());
            for (const auto& record : sbt.hit_group_records)
            {
                if (record)
                {
                    hit_group_record_size = std::max(hit_group_record_size, record->GetSize());
                }
            }
        }

        if (hit_group_record_count == 0)
        {
            hit_group_record_count = 1;
        }

        const size_t total_hit_group_records = ray_count * hit_group_record_count;
        build_table(
            L"HitGroupShaderTable",
            handle_size + hit_group_record_size,
            total_hit_group_records,
            [&](uint8_t* dst, size_t index, size_t stride)
            {
                const size_t sbt_index = index / hit_group_record_count;
                const size_t record_index = index % hit_group_record_count;
                const auto& sbt = sbts[sbt_index];
                const uint32_t group_index = vk_pso.GetShaderGroupIndex(sbt.hit_group_entry);
                memcpy(dst, get_group_handle(group_index), handle_size);
                if (record_index < sbt.hit_group_records.size() && sbt.hit_group_records[record_index])
                {
                    memcpy(dst + handle_size, sbt.hit_group_records[record_index]->GetData(), sbt.hit_group_records[record_index]->GetSize());
                }
                (void)stride;
            },
            m_hit_group_shader_table,
            m_hit_group_region);
    }

    m_callable_region = {};
    
    return true;
}
