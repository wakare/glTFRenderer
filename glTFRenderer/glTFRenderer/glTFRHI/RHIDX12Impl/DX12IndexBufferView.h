#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIIndexBufferView.h"

class DX12IndexBufferView : public IRHIIndexBufferView
{
public:
    DX12IndexBufferView();
    virtual ~DX12IndexBufferView() override;
    
    virtual bool InitIndexBufferView(IRHIGPUBuffer& buffer, size_t offset, RHIDataFormat indexFormat, size_t indexBufferSize) override;
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const override;
    virtual size_t GetSize() const override;
    virtual unsigned GetFormat() const override;
    
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const {return m_indexBufferView; }
    
private:
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
};
