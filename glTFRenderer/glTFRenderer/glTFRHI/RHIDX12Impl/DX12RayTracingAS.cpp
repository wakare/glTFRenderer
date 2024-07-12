#include "DX12RayTracingAS.h"

#include "DX12ConverterUtils.h"
#include "glTFRenderPass/glTFRenderMeshManager.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

DX12RayTracingAS::DX12RayTracingAS()
{
}

bool DX12RayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const glTFRenderMeshManager& mesh_manager, glTFRenderResourceManager
                                        & resource_manager)
{
    auto* dxr_device = dynamic_cast<DX12Device&>(device).GetDXRDevice();
    auto* dxr_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    const auto& mesh_render_resources = mesh_manager.GetMeshRenderResources();
    const auto& mesh_instances = mesh_manager.GetMeshInstanceRenderResource(); 
    
    m_blas_geometry_descs.resize(mesh_render_resources.size());
    m_blas_prebuild_infos.resize(mesh_render_resources.size());
    m_blas_build_inputs.resize(mesh_render_resources.size());
    
    unsigned mesh_index = 0;
    for (const auto& mesh_pair : mesh_render_resources)
    {
        auto& mesh = mesh_pair.second;
        RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, mesh.mesh_vertex_buffer->GetBuffer(), RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
        RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, mesh.mesh_index_buffer->GetBuffer(), RHIResourceStateType::STATE_INDEX_BUFFER, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);

        D3D12_RAYTRACING_GEOMETRY_DESC& geometry_desc = m_blas_geometry_descs[mesh_index];
        geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry_desc.Triangles.IndexBuffer = dynamic_cast<const DX12Buffer&>(mesh.mesh_index_buffer->GetBuffer()).GetGPUBufferHandle();
        geometry_desc.Triangles.IndexCount = mesh.mesh_index_count;
        geometry_desc.Triangles.IndexFormat = DX12ConverterUtils::ConvertToDXGIFormat(mesh.mesh_index_buffer->GetFormat());
        // Transform may cost much memory and calculation
        geometry_desc.Triangles.Transform3x4 = 0;
        geometry_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometry_desc.Triangles.VertexCount = mesh.mesh_vertex_count;
        geometry_desc.Triangles.VertexBuffer.StartAddress = dynamic_cast<const DX12Buffer&>(mesh.mesh_vertex_buffer->GetBuffer()).GetGPUBufferHandle();
        geometry_desc.Triangles.VertexBuffer.StrideInBytes = mesh.mesh_vertex_buffer->GetLayout().GetVertexStrideInBytes();
        // Mark the geometry as opaque. 
        // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
        // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
        geometry_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& bottom_level_prebuild_info = m_blas_prebuild_infos[mesh_index];
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottom_level_inputs = m_blas_build_inputs[mesh_index];
        bottom_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottom_level_inputs.Flags = build_flags;
        bottom_level_inputs.NumDescs = 1;
        bottom_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottom_level_inputs.pGeometryDescs = &geometry_desc;
        dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom_level_inputs, &bottom_level_prebuild_info);
        THROW_IF_FAILED(bottom_level_prebuild_info.ResultDataMaxSizeInBytes > 0)

        ++mesh_index;
    }

    // Get required sizes for an acceleration structure.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top_level_inputs = {};
    top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    top_level_inputs.Flags = build_flags;
    top_level_inputs.NumDescs = mesh_instances.size();
    top_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_level_prebuild_info = {};
    dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&top_level_inputs, &top_level_prebuild_info);
    THROW_IF_FAILED(top_level_prebuild_info.ResultDataMaxSizeInBytes > 0)

    // Init scratch buffer
    m_BLAS_scratch_buffers.resize(mesh_render_resources.size());
    {
        for (unsigned i = 0; i < m_blas_prebuild_infos.size(); ++i)
        {
            resource_manager.GetMemoryManager().AllocateBufferMemory(
            device,
        {
            L"blas_scratch_buffer",
            m_blas_prebuild_infos[i].ScratchDataSizeInBytes,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            RUF_ALLOW_UAV
        }, m_BLAS_scratch_buffers[i]);
        }
        resource_manager.GetMemoryManager().AllocateBufferMemory(
        device,
        {
            L"scratch_buffer",
            top_level_prebuild_info.ScratchDataSizeInBytes,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            RUF_ALLOW_UAV
    }, m_scratch_buffer);
    }

    unsigned blas_index = 0;
    m_BLASes.resize(mesh_render_resources.size());
    for (const auto& prebuild_info : m_blas_prebuild_infos)
    {
        resource_manager.GetMemoryManager().AllocateBufferMemory(
        device,
{
        L"BLAS",
        prebuild_info.ResultDataMaxSizeInBytes,
        1,
        1,
        RHIBufferType::Default,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        RUF_ALLOW_UAV
    },
    m_BLASes[blas_index++]
    );
    }
    resource_manager.GetMemoryManager().AllocateBufferMemory(
        device,
        {
            L"TLAS",
            top_level_prebuild_info.ResultDataMaxSizeInBytes,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            RUF_ALLOW_UAV
        },
        m_TLAS
    );
    m_TLAS_descriptor_allocation = RHIResourceFactory::CreateRHIResource<IRHIDescriptorAllocation>();
    m_TLAS_descriptor_allocation->InitFromBuffer(*m_TLAS->m_buffer,
        {
            .format = RHIDataFormat::UNKNOWN,
            .dimension = RHIResourceDimension::BUFFER,
            .view_type = RHIViewType::RVT_SRV,
        });

    // Create an instance desc for the bottom-level acceleration structure.
    m_instance_descs.resize(mesh_instances.size());
    blas_index = 0;
    for (const auto& instance : mesh_instances)
    {
        D3D12_RAYTRACING_INSTANCE_DESC& instance_desc = m_instance_descs[blas_index];
        memcpy(instance_desc.Transform, &instance.second.m_instance_transform[0][0], sizeof(instance_desc.Transform));
        instance_desc.InstanceID = blas_index;
        instance_desc.InstanceMask = 1;

        // Ray type count * instance offset 
        instance_desc.InstanceContributionToHitGroupIndex = blas_index;
        instance_desc.AccelerationStructure = dynamic_cast<const DX12Buffer&>(*m_BLASes[instance.second.m_mesh_render_resource]->m_buffer).GetGPUBufferHandle();
        ++blas_index;
    }

    resource_manager.GetMemoryManager().AllocateBufferMemory(device,
{
        L"Instance_desc_upload_buffer",
        m_instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        1,
        1,
        RHIBufferType::Upload,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_GENERIC_READ,
    },
m_upload_buffer);
    resource_manager.GetMemoryManager().UploadBufferData(*m_upload_buffer, m_instance_descs.data(), 0,  m_instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    
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
    top_level_inputs.InstanceDescs = dynamic_cast<const DX12Buffer&>(*m_upload_buffer->m_buffer).GetGPUBufferHandle();
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
            BLAS_barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(dynamic_cast<DX12Buffer&>(*m_BLAS->m_buffer).GetBuffer()));
        }
        
        raytracingCommandList->ResourceBarrier(BLAS_barriers.size(), BLAS_barriers.data());
        raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    };

    // Build acceleration structure.
    BuildAccelerationStructure(dxr_command_list);

    for (const auto& mesh_pair : mesh_render_resources)
    {
        auto& mesh = mesh_pair.second;
        RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, mesh.mesh_vertex_buffer->GetBuffer(), RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER);
        RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, mesh.mesh_index_buffer->GetBuffer(), RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_INDEX_BUFFER);
    }
    
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

