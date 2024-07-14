#include "DX12IndexBufferView.h"

#include "DX12ConverterUtils.h"
#include "DX12Buffer.h"
#include "DX12Utils.h"

DX12IndexBufferView::DX12IndexBufferView()
    : m_indexBufferView({})
{
}

DX12IndexBufferView::~DX12IndexBufferView()
{
}

bool DX12IndexBufferView::InitIndexBufferView(IRHIBuffer& buffer, size_t offset, RHIDataFormat indexFormat,
                                              size_t indexBufferSize)
{
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetRawBuffer();
    
    m_indexBufferView.BufferLocation = dxBuffer->GetGPUVirtualAddress() + offset;
    m_indexBufferView.Format = DX12ConverterUtils::ConvertToDXGIFormat(indexFormat);
    m_indexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);
    
    return true;
}

RHIGPUDescriptorHandle DX12IndexBufferView::GetGPUHandle() const
{
    return m_indexBufferView.BufferLocation;
}

size_t DX12IndexBufferView::GetSize() const
{
    return m_indexBufferView.SizeInBytes;
}

unsigned DX12IndexBufferView::GetFormat() const
{
    return m_indexBufferView.Format;
}