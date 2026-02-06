#include "DX12RayTracingAS.h"

#include "DX12Buffer.h"
#include "DX12CommandList.h"
#include "DX12ConverterUtils.h"
#include "DX12DescriptorManager.h"
#include "DX12Device.h"
#include "RHICommon.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/RHIIndexBuffer.h"
#include "RHIInterface/RHIVertexBuffer.h"

#include <cstring>

namespace
{
    DXGI_FORMAT GetPositionFormatFromLayout(const VertexLayoutDeclaration& layout)
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
                return DXGI_FORMAT_R32G32_FLOAT;
            case 12:
                return DXGI_FORMAT_R32G32B32_FLOAT;
            case 16:
                return DXGI_FORMAT_R32G32B32A32_FLOAT;
            default:
                return DXGI_FORMAT_R32G32B32_FLOAT;
            }
        }

        return DXGI_FORMAT_R32G32B32_FLOAT;
    }

    size_t SafeNonZeroSize(size_t size)
    {
        return size == 0 ? 1 : size;
    }
}

DX12RayTracingAS::DX12RayTracingAS()
{
}

void DX12RayTracingAS::SetRayTracingSceneDesc(const RHIRayTracingSceneDesc& scene_desc)
{
    m_scene_desc = scene_desc;
}

bool DX12RayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager&
                                        memory_manager)
{
    auto* dxr_device = dynamic_cast<DX12Device&>(device).GetDXRDevice();
    auto* dxr_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();

    const auto build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    const auto& geometries = m_scene_desc.geometries;
    const auto& instances = m_scene_desc.instances;

    m_blas_geometry_descs.resize(geometries.size());
    m_blas_prebuild_infos.resize(geometries.size());
    m_blas_build_inputs.resize(geometries.size());

    unsigned mesh_index = 0;
    for (const auto& geometry : geometries)
    {
        GLTF_CHECK(geometry.vertex_buffer);
        GLTF_CHECK(geometry.index_buffer);

        D3D12_RAYTRACING_GEOMETRY_DESC& geometry_desc = m_blas_geometry_descs[mesh_index];
        geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry_desc.Triangles.IndexBuffer = dynamic_cast<const DX12Buffer&>(geometry.index_buffer->GetBuffer()).GetGPUBufferHandle();
        geometry_desc.Triangles.IndexCount = static_cast<UINT>(geometry.index_count);
        geometry_desc.Triangles.IndexFormat = DX12ConverterUtils::ConvertToDXGIFormat(geometry.index_buffer->GetFormat());
        // Transform may cost much memory and calculation
        geometry_desc.Triangles.Transform3x4 = 0;
        geometry_desc.Triangles.VertexFormat = GetPositionFormatFromLayout(geometry.vertex_buffer->GetLayout());
        geometry_desc.Triangles.VertexCount = static_cast<UINT>(geometry.vertex_count);
        geometry_desc.Triangles.VertexBuffer.StartAddress = dynamic_cast<const DX12Buffer&>(geometry.vertex_buffer->GetBuffer()).GetGPUBufferHandle();
        geometry_desc.Triangles.VertexBuffer.StrideInBytes = geometry.vertex_buffer->GetLayout().GetVertexStrideInBytes();
        // Mark the geometry as opaque.
        geometry_desc.Flags = geometry.opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& bottom_level_prebuild_info = m_blas_prebuild_infos[mesh_index];
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottom_level_inputs = m_blas_build_inputs[mesh_index];
        bottom_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottom_level_inputs.Flags = build_flags;
        bottom_level_inputs.NumDescs = 1;
        bottom_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottom_level_inputs.pGeometryDescs = &geometry_desc;
        dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom_level_inputs, &bottom_level_prebuild_info);
        GLTF_CHECK(bottom_level_prebuild_info.ResultDataMaxSizeInBytes > 0);

        ++mesh_index;
    }

    // Get required sizes for an acceleration structure.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top_level_inputs = {};
    top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    top_level_inputs.Flags = build_flags;
    top_level_inputs.NumDescs = static_cast<UINT>(instances.size());
    top_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_level_prebuild_info = {};
    dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&top_level_inputs, &top_level_prebuild_info);
    const size_t tlas_result_size = SafeNonZeroSize(top_level_prebuild_info.ResultDataMaxSizeInBytes);
    const size_t tlas_scratch_size = SafeNonZeroSize(top_level_prebuild_info.ScratchDataSizeInBytes);

    // Init scratch buffer
    m_BLAS_scratch_buffers.resize(geometries.size());
    {
        for (unsigned i = 0; i < m_blas_prebuild_infos.size(); ++i)
        {
            memory_manager.AllocateBufferMemory(
            device,
        {
            L"blas_scratch_buffer",
            m_blas_prebuild_infos[i].ScratchDataSizeInBytes,
            1,
            1,
            RHIBufferType::Default,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_UNORDERED_ACCESS,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV)
        }, m_BLAS_scratch_buffers[i]);
        }
        memory_manager.AllocateBufferMemory(
        device,
        {
            L"scratch_buffer",
            tlas_scratch_size,
            1,
            1,
            RHIBufferType::Default,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_UNORDERED_ACCESS,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV)
    }, m_scratch_buffer);
    }

    unsigned blas_index = 0;
    m_BLASes.resize(geometries.size());
    for (const auto& prebuild_info : m_blas_prebuild_infos)
    {
        memory_manager.AllocateBufferMemory(
        device,
{
        L"BLAS",
        prebuild_info.ResultDataMaxSizeInBytes,
        1,
        1,
        RHIBufferType::Default,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV)
    },
    m_BLASes[blas_index++]
    );
    }
    memory_manager.AllocateBufferMemory(
        device,
        {
            L"TLAS",
            tlas_result_size,
            1,
            1,
            RHIBufferType::Default,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV)
        },
        m_TLAS
    );
    m_TLAS_descriptor_allocation = RHIResourceFactory::CreateRHIResource<IRHIAccelerationStructureDescriptorAllocation>();
    DX12Buffer& dx12_buffer = dynamic_cast<DX12Buffer&>(*m_TLAS->m_buffer);
    dynamic_cast<DX12AccelerationStructureDescriptorAllocation&>(*m_TLAS_descriptor_allocation)
        .InitFromAccelerationStructure(dx12_buffer.GetRawBuffer()->GetGPUVirtualAddress());

    // Create an instance desc for the bottom-level acceleration structure.
    if (!instances.empty())
    {
        m_instance_descs.resize(instances.size());
        for (size_t i = 0; i < instances.size(); ++i)
        {
            const auto& instance = instances[i];
            D3D12_RAYTRACING_INSTANCE_DESC& instance_desc = m_instance_descs[i];
            memcpy(instance_desc.Transform, instance.transform.data(), sizeof(instance_desc.Transform));
            instance_desc.InstanceID = instance.instance_id;
            instance_desc.InstanceMask = static_cast<UINT>(instance.instance_mask);
            instance_desc.InstanceContributionToHitGroupIndex = instance.hit_group_index;
            GLTF_CHECK(instance.geometry_index < m_BLASes.size());
            instance_desc.AccelerationStructure = dynamic_cast<const DX12Buffer&>(*m_BLASes[instance.geometry_index]->m_buffer).GetGPUBufferHandle();
        }

        memory_manager.AllocateBufferMemory(device,
    {
            L"Instance_desc_upload_buffer",
            m_instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
            1,
            1,
            RHIBufferType::Upload,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_GENERIC_READ,
        },
    m_upload_buffer);
        memory_manager.UploadBufferData(device, command_list, *m_upload_buffer, m_instance_descs.data(), 0,
            m_instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    }
    
    // Bottom Level Acceleration Structure desc
    std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> BLAS_build_descs(m_blas_build_inputs.size());
    blas_index = 0;
    for (const auto& build_level_input : m_blas_build_inputs)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& bottomLevelBuildDesc = BLAS_build_descs[blas_index];
        bottomLevelBuildDesc.Inputs = build_level_input;
        bottomLevelBuildDesc.ScratchAccelerationStructureData = dynamic_cast<const DX12Buffer&>(*m_BLAS_scratch_buffers[blas_index]->m_buffer).GetGPUBufferHandle();
        bottomLevelBuildDesc.DestAccelerationStructureData = dynamic_cast<const DX12Buffer&>(*m_BLASes[blas_index]->m_buffer).GetGPUBufferHandle();
        ++blas_index;
    }

    // Top Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    top_level_inputs.InstanceDescs = instances.empty() ? 0 : dynamic_cast<const DX12Buffer&>(*m_upload_buffer->m_buffer).GetGPUBufferHandle();
    topLevelBuildDesc.Inputs = top_level_inputs;
    topLevelBuildDesc.ScratchAccelerationStructureData = dynamic_cast<const DX12Buffer&>(*m_scratch_buffer->m_buffer).GetGPUBufferHandle();
    topLevelBuildDesc.DestAccelerationStructureData = dynamic_cast<const DX12Buffer&>(*m_TLAS->m_buffer).GetGPUBufferHandle();

    auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> BLAS_barriers;
        BLAS_barriers.reserve(m_BLASes.size());
        blas_index = 0;
        for (auto& m_BLAS : m_BLASes)
        {
            raytracingCommandList->BuildRaytracingAccelerationStructure(&BLAS_build_descs[blas_index++], 0, nullptr);
            BLAS_barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(dynamic_cast<DX12Buffer&>(*m_BLAS->m_buffer).GetRawBuffer()));
        }

        if (!BLAS_barriers.empty())
        {
            raytracingCommandList->ResourceBarrier(static_cast<UINT>(BLAS_barriers.size()), BLAS_barriers.data());
        }
        raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    };

    // Build acceleration structure.
    BuildAccelerationStructure(dxr_command_list);
    return true;
}

const IRHIDescriptorAllocation& DX12RayTracingAS::GetTLASDescriptorSRV() const
{
    return *m_TLAS_descriptor_allocation;
}

const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& DX12RayTracingAS::GetInstanceDesc() const
{
    return m_instance_descs;
}
