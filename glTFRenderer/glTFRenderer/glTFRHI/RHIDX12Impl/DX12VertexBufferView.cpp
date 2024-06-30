#include "DX12VertexBufferView.h"

#include "DX12Buffer.h"

DX12VertexBufferView::DX12VertexBufferView()
    : m_vertexBufferView({})
{
}

DX12VertexBufferView::~DX12VertexBufferView()
{
}

void DX12VertexBufferView::InitVertexBufferView(IRHIBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize)
{
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetBuffer();
    m_vertexBufferView.BufferLocation = dxBuffer->GetGPUVirtualAddress() + offset;
    m_vertexBufferView.StrideInBytes = static_cast<UINT>(vertexStride);
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);
}

RHIGPUDescriptorHandle DX12VertexBufferView::GetGPUHandle() const
{
    return m_vertexBufferView.BufferLocation;
}

size_t DX12VertexBufferView::GetSize() const
{
    return m_vertexBufferView.SizeInBytes;
}

size_t DX12VertexBufferView::GetStride() const
{
    return m_vertexBufferView.StrideInBytes;
}
