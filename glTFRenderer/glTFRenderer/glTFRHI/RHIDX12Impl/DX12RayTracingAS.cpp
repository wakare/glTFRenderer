#include "DX12RayTracingAS.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

DX12RayTracingAS::DX12RayTracingAS()
{
    m_TLAS = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_scratch_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
}

bool DX12RayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const std::map<glTFUniqueID, glTFRenderPassMeshResource>& meshes)
{
    auto* dxr_device = dynamic_cast<DX12Device&>(device).GetDXRDevice();
    auto* dxr_command_list = dynamic_cast<DX12CommandList&>(command_list).GetDXRCommandList();
    
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    m_blas_geometry_descs.resize(meshes.size());
    m_blas_prebuild_infos.resize(meshes.size());
    m_blas_build_inputs.resize(meshes.size());
    
    unsigned mesh_index = 0;
    for (const auto& mesh_pair : meshes)
    {
        auto& mesh = mesh_pair.second;
        D3D12_RAYTRACING_GEOMETRY_DESC& geometry_desc = m_blas_geometry_descs[mesh_index];
        geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry_desc.Triangles.IndexBuffer = mesh.mesh_index_buffer->GetBuffer().GetGPUBufferHandle();
        geometry_desc.Triangles.IndexCount = mesh.mesh_index_count;
        geometry_desc.Triangles.IndexFormat = DX12ConverterUtils::ConvertToDXGIFormat(mesh.mesh_index_buffer->GetFormat());
        // Transform may cost much memory and calculation
        geometry_desc.Triangles.Transform3x4 = 0;
        geometry_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometry_desc.Triangles.VertexCount = mesh.mesh_vertex_count;
        geometry_desc.Triangles.VertexBuffer.StartAddress = mesh.mesh_vertex_buffer->GetBuffer().GetGPUBufferHandle();
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
    top_level_inputs.NumDescs = meshes.size();
    top_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_level_prebuild_info = {};
    dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&top_level_inputs, &top_level_prebuild_info);
    THROW_IF_FAILED(top_level_prebuild_info.ResultDataMaxSizeInBytes > 0)

    // Init scratch buffer
    m_BLAS_scratch_buffers.resize(meshes.size());
    {
        for (unsigned i = 0; i < m_blas_prebuild_infos.size(); ++i)
        {
            m_BLAS_scratch_buffers[i] = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
            m_BLAS_scratch_buffers[i]->InitGPUBuffer(device,
            {
                L"blas_scratch_buffer",
                m_blas_prebuild_infos[i].ScratchDataSizeInBytes,
                1,
                1,
                RHIBufferType::Default,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_UNORDER_ACCESS,
                RHIBufferUsage::ALLOW_UNORDER_ACCESS
            });
        }
        
        m_scratch_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        m_scratch_buffer->InitGPUBuffer(device,
            {
                L"scratch_buffer",
                top_level_prebuild_info.ScratchDataSizeInBytes,
                1,
                1,
                RHIBufferType::Default,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_UNORDER_ACCESS,
                RHIBufferUsage::ALLOW_UNORDER_ACCESS
        });    
    }

    unsigned blas_index = 0;
    m_BLASes.resize(meshes.size());
    for (const auto& prebuild_info : m_blas_prebuild_infos)
    {
        m_BLASes[blas_index] = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        RETURN_IF_FALSE(m_BLASes[blas_index++]->InitGPUBuffer(device,
            {
                L"BLAS",
                prebuild_info.ResultDataMaxSizeInBytes,
                1,
                1,
                RHIBufferType::Default,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                RHIBufferUsage::ALLOW_UNORDER_ACCESS
            }
        ))    
    }

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
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instance_descs(meshes.size());
    blas_index = 0;
    for (const auto& mesh_pair : meshes)
    {
        D3D12_RAYTRACING_INSTANCE_DESC& instance_desc = instance_descs[blas_index];
        memcpy(instance_desc.Transform, &mesh_pair.second.mesh_transform_matrix[0][0], sizeof(instance_desc.Transform));
        instance_desc.InstanceID = blas_index;
        instance_desc.InstanceMask = 1;
        instance_desc.AccelerationStructure = m_BLASes[blas_index]->GetGPUBufferHandle();
        ++blas_index;
    }
    
    m_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_upload_buffer->InitGPUBuffer(device,
        {
            L"Instance_desc_upload_buffer",
            instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_GENERIC_READ,
            RHIBufferUsage::NONE
        }))
    
    m_upload_buffer->UploadBufferFromCPU(instance_descs.data(), 0,  instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    
    // Bottom Level Acceleration Structure desc
    std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> BLAS_build_descs(m_blas_build_inputs.size());
    blas_index = 0;
    for (const auto& build_level_input : m_blas_build_inputs)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& bottomLevelBuildDesc = BLAS_build_descs[blas_index];
        bottomLevelBuildDesc.Inputs = build_level_input;
        bottomLevelBuildDesc.ScratchAccelerationStructureData = m_BLAS_scratch_buffers[blas_index]->GetGPUBufferHandle();
        bottomLevelBuildDesc.DestAccelerationStructureData = m_BLASes[blas_index]->GetGPUBufferHandle();
        ++blas_index;
    }

    // Top Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    top_level_inputs.InstanceDescs = m_upload_buffer->GetGPUBufferHandle();
    topLevelBuildDesc.Inputs = top_level_inputs;
    topLevelBuildDesc.ScratchAccelerationStructureData = m_scratch_buffer->GetGPUBufferHandle();
    topLevelBuildDesc.DestAccelerationStructureData = m_TLAS->GetGPUBufferHandle();

    auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> BLAS_barriers;
        BLAS_barriers.reserve(m_BLASes.size());
        blas_index = 0;
        for (auto& m_BLAS : m_BLASes)
        {
            raytracingCommandList->BuildRaytracingAccelerationStructure(&BLAS_build_descs[blas_index++], 0, nullptr);
            BLAS_barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(dynamic_cast<DX12GPUBuffer*>(m_BLAS.get())->GetBuffer()));
        }
        
        raytracingCommandList->ResourceBarrier(BLAS_barriers.size(), BLAS_barriers.data());
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

