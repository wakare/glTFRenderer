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

bool DX12IndexBufferView::InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc)
{
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetRawBuffer();
    
    m_indexBufferView.BufferLocation = dxBuffer->GetGPUVirtualAddress() + desc.offset;
    m_indexBufferView.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.format);
    m_indexBufferView.SizeInBytes = static_cast<UINT>(desc.size);

    m_desc = desc;
    
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