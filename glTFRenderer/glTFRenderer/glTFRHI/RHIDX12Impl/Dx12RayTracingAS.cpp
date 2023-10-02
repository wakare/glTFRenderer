#include "Dx12RayTracingAS.h"

bool Dx12RayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIGPUBuffer& vertex_buffer, IRHIGPUBuffer& index_buffer)
{
    /*
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.IndexBuffer = index_buffer.GetGPUBufferHandle();
    geometryDesc.Triangles.IndexCount = static_cast<UINT>(index_buffer.GetBufferDesc().width) / sizeof(Index);
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
    geometryDesc.Triangles.Transform3x4 = 0;
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount = static_cast<UINT>(m_vertexBuffer->GetDesc().Width) / sizeof(Vertex);
    geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    */
    return true;
}

GPU_BUFFER_HANDLE_TYPE Dx12RayTracingAS::GetTLASHandle() const
{
    GLTF_CHECK(m_TLAS);
    return m_TLAS->GetGPUVirtualAddress();
}
