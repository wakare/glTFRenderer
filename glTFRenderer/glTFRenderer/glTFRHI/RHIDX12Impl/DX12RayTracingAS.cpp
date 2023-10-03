#include "DX12RayTracingAS.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

DX12RayTracingAS::DX12RayTracingAS()
{
    m_BLAS = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_TLAS = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_scratch_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
}

bool DX12RayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIVertexBuffer& vertex_buffer, IRHIIndexBuffer& index_buffer)
{
    auto* dxr_device = dynamic_cast<DX12Device&>(device).GetDXRDevice();
    auto* dxr_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    
    D3D12_RAYTRACING_GEOMETRY_DESC geometry_desc = {};
    geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometry_desc.Triangles.IndexBuffer = index_buffer.GetBuffer().GetGPUBufferHandle();
    geometry_desc.Triangles.IndexCount = index_buffer.GetCount();
    geometry_desc.Triangles.IndexFormat = DX12ConverterUtils::ConvertToDXGIFormat(index_buffer.GetFormat());
    geometry_desc.Triangles.Transform3x4 = 0;
    geometry_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometry_desc.Triangles.VertexCount = vertex_buffer.GetCount();
    geometry_desc.Triangles.VertexBuffer.StartAddress = vertex_buffer.GetBuffer().GetGPUBufferHandle();
    geometry_desc.Triangles.VertexBuffer.StrideInBytes = vertex_buffer.GetLayout().GetVertexStrideInBytes();
    // Mark the geometry as opaque. 
    // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
    // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
    geometry_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    // Get required sizes for an acceleration structure.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top_level_inputs = {};
    top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    top_level_inputs.Flags = build_flags;
    top_level_inputs.NumDescs = 1;
    top_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_level_prebuild_info = {};
    dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&top_level_inputs, &top_level_prebuild_info);
    THROW_IF_FAILED(top_level_prebuild_info.ResultDataMaxSizeInBytes > 0)

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottom_level_prebuild_info = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom_level_inputs = top_level_inputs;
    bottom_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottom_level_inputs.pGeometryDescs = &geometry_desc;
    dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom_level_inputs, &bottom_level_prebuild_info);
    THROW_IF_FAILED(bottom_level_prebuild_info.ResultDataMaxSizeInBytes > 0)

    // Init scratch buffer
    const size_t scratch_buffer_size = max(top_level_prebuild_info.ScratchDataSizeInBytes, bottom_level_prebuild_info.ScratchDataSizeInBytes);
    m_scratch_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_scratch_buffer->InitGPUBuffer(device,
        {
            L"scratch_buffer",
            scratch_buffer_size,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_UNORDER_ACCESS,
            RHIBufferUsage::ALLOW_UNORDER_ACCESS
    });
    
    RETURN_IF_FALSE(m_BLAS->InitGPUBuffer(device,
        {
            L"BLAS",
            bottom_level_prebuild_info.ResultDataMaxSizeInBytes,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            RHIBufferUsage::ALLOW_UNORDER_ACCESS
        }
    ))

    RETURN_IF_FALSE(m_TLAS->InitGPUBuffer(device,
        {
            L"TLAS",
            top_level_prebuild_info.ResultDataMaxSizeInBytes,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            RHIBufferUsage::ALLOW_UNORDER_ACCESS
        }
    ))

    // Create an instance desc for the bottom-level acceleration structure.
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
    instanceDesc.InstanceMask = 1;
    instanceDesc.AccelerationStructure = m_BLAS->GetGPUBufferHandle();
    m_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_upload_buffer->InitGPUBuffer(device,
        {
            L"Instance_desc_upload_buffer",
            sizeof(instanceDesc),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_GENERIC_READ,
            RHIBufferUsage::NONE
        }))
    
    m_upload_buffer->UploadBufferFromCPU(&instanceDesc, 0, sizeof(instanceDesc));
    // Bottom Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    {
        bottomLevelBuildDesc.Inputs = bottom_level_inputs;
        bottomLevelBuildDesc.ScratchAccelerationStructureData = m_scratch_buffer->GetGPUBufferHandle();
        bottomLevelBuildDesc.DestAccelerationStructureData = m_BLAS->GetGPUBufferHandle();
    }

    // Top Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    {
        top_level_inputs.InstanceDescs = m_upload_buffer->GetGPUBufferHandle();
        topLevelBuildDesc.Inputs = top_level_inputs;
        topLevelBuildDesc.ScratchAccelerationStructureData = m_scratch_buffer->GetGPUBufferHandle();
        topLevelBuildDesc.DestAccelerationStructureData = m_TLAS->GetGPUBufferHandle();
    }

    auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(dynamic_cast<DX12GPUBuffer*>(m_BLAS.get())->GetBuffer());
        raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
        raytracingCommandList->ResourceBarrier(1, &barrier);
        raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    };

    // Build acceleration structure.
    BuildAccelerationStructure(dxr_command_list);
    
    return true;
}

GPU_BUFFER_HANDLE_TYPE DX12RayTracingAS::GetTLASHandle() const
{
    GLTF_CHECK(m_TLAS);
    return m_TLAS->GetGPUBufferHandle();
}
