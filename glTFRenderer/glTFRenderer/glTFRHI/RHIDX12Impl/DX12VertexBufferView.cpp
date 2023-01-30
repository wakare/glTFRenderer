#include "DX12VertexBufferView.h"

#include "DX12GPUBuffer.h"

DX12VertexBufferView::DX12VertexBufferView()
    : m_vertexBufferView({})
{
}

DX12VertexBufferView::~DX12VertexBufferView()
{
}

void DX12VertexBufferView::InitVertexBufferView(IRHIGPUBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize)
{
    auto* dxBuffer = dynamic_cast<DX12GPUBuffer&>(buffer).GetBuffer();
    m_vertexBufferView.BufferLocation = dxBuffer->GetGPUVirtualAddress() + offset;
    m_vertexBufferView.StrideInBytes = static_cast<UINT>(vertexStride);
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);
}
