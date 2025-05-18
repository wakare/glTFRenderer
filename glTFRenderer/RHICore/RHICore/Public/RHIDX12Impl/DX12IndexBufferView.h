#pragma once
#include <d3d12.h>

#include "IRHIIndexBufferView.h"

class DX12IndexBufferView : public IRHIIndexBufferView
{
public:
    DX12IndexBufferView();
    virtual ~DX12IndexBufferView() override;
    
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) override;
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const;
    virtual size_t GetSize() const;
    virtual unsigned GetFormat() const;
    
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const {return m_indexBufferView; }
    
private:
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
};

struct RHIIndirectArgumentIndexBufferView
{
    RHIIndirectArgumentIndexBufferView(const DX12IndexBufferView& index_buffer_view)
        : handle(index_buffer_view.GetGPUHandle())
        , size(index_buffer_view.GetSize())
        , format(index_buffer_view.GetFormat())
    {
        
    }
    
    RHIGPUDescriptorHandle handle;
    unsigned size;
    unsigned format; 
};
