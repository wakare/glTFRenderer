#include "VKRayTracingAS.h"

#include "VKDescriptorManager.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKBuffer.h"
#include "VKCommandList.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/RHIIndexBuffer.h"
#include "RHIInterface/RHIVertexBuffer.h"

#include <cstring>

void VKRayTracingAS::SetRayTracingSceneDesc(const RHIRayTracingSceneDesc& scene_desc)
{
    m_scene_desc = scene_desc;
}

bool VKRayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list,
                                      IRHIMemoryManager& memory_manager)
{
    if (!m_tlas_descriptor_allocation)
    {
        auto allocation = RHIResourceFactory::CreateRHIResource<IRHIAccelerationStructureDescriptorAllocation>();
        auto& vk_allocation = dynamic_cast<VKAccelerationStructureDescriptorAllocation&>(*allocation);
        vk_allocation.InitFromAccelerationStructure(0);
        m_tlas_descriptor_allocation = allocation;
    }

    auto& vk_device = dynamic_cast<VKDevice&>(device);
    VkDevice raw_device = vk_device.GetDevice();
    VkCommandBuffer raw_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();

    const auto& geometries = m_scene_desc.geometries;
    const auto& instances = m_scene_desc.instances;
    if (geometries.empty())
    {
        return true;
    }

    auto get_buffer_device_address = [&](VkBuffer buffer) -> VkDeviceAddress
    {
        VkBufferDeviceAddressInfo address_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        address_info.buffer = buffer;
        return vkGetBufferDeviceAddress(raw_device, &address_info);
    };

    auto get_acceleration_structure_address = [&](VkAccelerationStructureKHR as) -> VkDeviceAddress
    {
        VkAccelerationStructureDeviceAddressInfoKHR address_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        address_info.accelerationStructure = as;
        return vkGetAccelerationStructureDeviceAddressKHR(raw_device, &address_info);
    };

    auto get_vertex_format = [](const VertexLayoutDeclaration& layout) -> VkFormat
    {
        for (const auto& element : layout.elements)
        {
            if (element.type != VertexAttributeType::VERTEX_POSITION)
            {
                continue;
            }

            switch (element.byte_size)
            {
            case 8:
                return VK_FORMAT_R32G32_SFLOAT;
            case 12:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case 16:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            default:
                return VK_FORMAT_R32G32B32_SFLOAT;
            }
        }

        return VK_FORMAT_R32G32B32_SFLOAT;
    };

    auto get_index_type = [](RHIDataFormat format) -> VkIndexType
    {
        switch (format)
        {
        case RHIDataFormat::R16_UINT:
            return VK_INDEX_TYPE_UINT16;
        case RHIDataFormat::R32_UINT:
            return VK_INDEX_TYPE_UINT32;
        default:
            return VK_INDEX_TYPE_UINT32;
        }
    };

    auto safe_non_zero = [](size_t size) -> size_t
    {
        return size == 0 ? 1 : size;
    };

    m_blas.clear();
    m_blas_buffers.clear();
    m_blas_scratch_buffers.clear();
    m_blas.reserve(geometries.size());
    m_blas_buffers.resize(geometries.size());
    m_blas_scratch_buffers.resize(geometries.size());

    std::vector<VkAccelerationStructureGeometryKHR> blas_geometries(geometries.size());
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blas_build_infos(geometries.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> blas_ranges(geometries.size());

    for (size_t i = 0; i < geometries.size(); ++i)
    {
        const auto& geometry = geometries[i];
        GLTF_CHECK(geometry.vertex_buffer);
        GLTF_CHECK(geometry.index_buffer);

        VkBuffer vertex_buffer = dynamic_cast<const VKBuffer&>(geometry.vertex_buffer->GetBuffer()).GetRawBuffer();
        VkBuffer index_buffer = dynamic_cast<const VKBuffer&>(geometry.index_buffer->GetBuffer()).GetRawBuffer();
        VkDeviceAddress vertex_address = get_buffer_device_address(vertex_buffer);
        VkDeviceAddress index_address = get_buffer_device_address(index_buffer);

        VkAccelerationStructureGeometryKHR geometry_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geometry_info.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry_info.flags = geometry.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

        auto& triangles = geometry_info.geometry.triangles;
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = get_vertex_format(geometry.vertex_buffer->GetLayout());
        triangles.vertexData.deviceAddress = vertex_address;
        triangles.vertexStride = geometry.vertex_buffer->GetLayout().GetVertexStrideInBytes();
        triangles.maxVertex = static_cast<uint32_t>(geometry.vertex_count);
        triangles.indexType = get_index_type(geometry.index_buffer->GetFormat());
        triangles.indexData.deviceAddress = index_address;
        triangles.transformData.deviceAddress = 0;

        blas_geometries[i] = geometry_info;

        VkAccelerationStructureBuildGeometryInfoKHR build_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build_info.geometryCount = 1;
        build_info.pGeometries = &blas_geometries[i];
        build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        uint32_t primitive_count = geometry.index_count > 0 ? static_cast<uint32_t>(geometry.index_count / 3) : 0;
        VkAccelerationStructureBuildSizesInfoKHR size_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(raw_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &primitive_count, &size_info);

        RHIBufferDesc blas_buffer_desc
        {
            L"BLAS",
            safe_non_zero(size_info.accelerationStructureSize),
            1,
            1,
            RHIBufferType::Default,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_RAY_TRACING),
        };

        memory_manager.AllocateBufferMemory(device, blas_buffer_desc, m_blas_buffers[i]);
        VkBuffer as_buffer = dynamic_cast<VKBuffer&>(*m_blas_buffers[i]->m_buffer).GetRawBuffer();

        VkAccelerationStructureCreateInfoKHR create_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        create_info.buffer = as_buffer;
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        VkAccelerationStructureKHR blas_handle = VK_NULL_HANDLE;
        VK_CHECK(vkCreateAccelerationStructureKHR(raw_device, &create_info, nullptr, &blas_handle));
        m_blas.push_back(blas_handle);

        RHIBufferDesc scratch_desc
        {
            L"BLAS_Scratch",
            safe_non_zero(size_info.buildScratchSize),
            1,
            1,
            RHIBufferType::Default,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_UNORDERED_ACCESS,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_RAY_TRACING),
        };
        memory_manager.AllocateBufferMemory(device, scratch_desc, m_blas_scratch_buffers[i]);

        build_info.dstAccelerationStructure = blas_handle;
        blas_build_infos[i] = build_info;

        VkAccelerationStructureBuildRangeInfoKHR range_info{};
        range_info.primitiveCount = primitive_count;
        range_info.primitiveOffset = 0;
        range_info.firstVertex = 0;
        range_info.transformOffset = 0;
        blas_ranges[i] = range_info;
    }

    for (size_t i = 0; i < blas_build_infos.size(); ++i)
    {
        VkAccelerationStructureBuildGeometryInfoKHR& build_info = blas_build_infos[i];
        VkBuffer scratch_buffer = dynamic_cast<VKBuffer&>(*m_blas_scratch_buffers[i]->m_buffer).GetRawBuffer();
        build_info.scratchData.deviceAddress = get_buffer_device_address(scratch_buffer);

        const VkAccelerationStructureBuildRangeInfoKHR* range_ptr = &blas_ranges[i];
        vkCmdBuildAccelerationStructuresKHR(raw_command_buffer, 1, &build_info, &range_ptr);
    }

    if (!m_blas.empty())
    {
        VkMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

        VkDependencyInfo dep_info{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep_info.memoryBarrierCount = 1;
        dep_info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(raw_command_buffer, &dep_info);
    }

    std::vector<VkAccelerationStructureInstanceKHR> vk_instances;
    vk_instances.reserve(instances.size());
    for (const auto& instance : instances)
    {
        VkAccelerationStructureInstanceKHR vk_instance{};
        VkTransformMatrixKHR transform{};
        memcpy(transform.matrix, instance.transform.data(), sizeof(transform.matrix));
        vk_instance.transform = transform;
        vk_instance.instanceCustomIndex = instance.instance_id;
        vk_instance.mask = static_cast<uint8_t>(instance.instance_mask);
        vk_instance.instanceShaderBindingTableRecordOffset = instance.hit_group_index;
        vk_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        GLTF_CHECK(instance.geometry_index < m_blas.size());
        vk_instance.accelerationStructureReference = get_acceleration_structure_address(m_blas[instance.geometry_index]);
        vk_instances.push_back(vk_instance);
    }

    VkDeviceAddress instance_buffer_address = 0;
    if (!vk_instances.empty())
    {
        RHIBufferDesc instance_desc
        {
            L"TLAS_InstanceBuffer",
            vk_instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
            1,
            1,
            RHIBufferType::Upload,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_GENERIC_READ,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_RAY_TRACING),
        };
        memory_manager.AllocateBufferMemory(device, instance_desc, m_instance_buffer);
        memory_manager.UploadBufferData(device, command_list, *m_instance_buffer, vk_instances.data(), 0,
            vk_instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

        VkBuffer instance_buffer = dynamic_cast<VKBuffer&>(*m_instance_buffer->m_buffer).GetRawBuffer();
        instance_buffer_address = get_buffer_device_address(instance_buffer);
    }

    VkAccelerationStructureGeometryKHR tlas_geometry{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    tlas_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlas_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlas_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    tlas_geometry.geometry.instances.data.deviceAddress = instance_buffer_address;

    VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    tlas_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlas_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlas_build_info.geometryCount = 1;
    tlas_build_info.pGeometries = &tlas_geometry;
    tlas_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    uint32_t instance_count = static_cast<uint32_t>(vk_instances.size());
    VkAccelerationStructureBuildSizesInfoKHR tlas_size_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(raw_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlas_build_info, &instance_count, &tlas_size_info);

    RHIBufferDesc tlas_buffer_desc
    {
        L"TLAS",
        safe_non_zero(tlas_size_info.accelerationStructureSize),
        1,
        1,
        RHIBufferType::Default,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_RAY_TRACING),
    };
    memory_manager.AllocateBufferMemory(device, tlas_buffer_desc, m_tlas_buffer);
    VkBuffer tlas_buffer = dynamic_cast<VKBuffer&>(*m_tlas_buffer->m_buffer).GetRawBuffer();

    VkAccelerationStructureCreateInfoKHR tlas_create_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    tlas_create_info.buffer = tlas_buffer;
    tlas_create_info.offset = 0;
    tlas_create_info.size = tlas_size_info.accelerationStructureSize;
    tlas_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(vkCreateAccelerationStructureKHR(raw_device, &tlas_create_info, nullptr, &m_tlas));

    RHIBufferDesc tlas_scratch_desc
    {
        L"TLAS_Scratch",
        safe_non_zero(tlas_size_info.buildScratchSize),
        1,
        1,
        RHIBufferType::Default,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_UNORDERED_ACCESS,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_RAY_TRACING),
    };
    memory_manager.AllocateBufferMemory(device, tlas_scratch_desc, m_tlas_scratch_buffer);

    VkBuffer tlas_scratch_buffer = dynamic_cast<VKBuffer&>(*m_tlas_scratch_buffer->m_buffer).GetRawBuffer();
    tlas_build_info.dstAccelerationStructure = m_tlas;
    tlas_build_info.scratchData.deviceAddress = get_buffer_device_address(tlas_scratch_buffer);

    VkAccelerationStructureBuildRangeInfoKHR tlas_range{};
    tlas_range.primitiveCount = instance_count;

    const VkAccelerationStructureBuildRangeInfoKHR* tlas_range_ptr = &tlas_range;
    vkCmdBuildAccelerationStructuresKHR(raw_command_buffer, 1, &tlas_build_info, &tlas_range_ptr);

    dynamic_cast<VKAccelerationStructureDescriptorAllocation&>(*m_tlas_descriptor_allocation).InitFromAccelerationStructure(
        reinterpret_cast<uint64_t>(m_tlas));
    return true;
}

const IRHIDescriptorAllocation& VKRayTracingAS::GetTLASDescriptorSRV() const
{
    GLTF_CHECK(m_tlas_descriptor_allocation);
    return *m_tlas_descriptor_allocation;
}
