#pragma once
#include <d3d12.h>

#include "RHIInterface/IRHIVertexBufferView.h"

class RHICORE_API DX12VertexBufferView : public IRHIVertexBufferView
{
public:
    DX12VertexBufferView();
    virtual ~DX12VertexBufferView() override;
    
    virtual void InitVertexBufferView(IRHIBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) override;

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const {return m_vertexBufferView; }
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const;
    virtual size_t GetSize() const;
    virtual size_t GetStride() const;
    
private:
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};


struct RHIIndirectArgumentVertexBufferView
{
    RHIIndirectArgumentVertexBufferView(const DX12VertexBufferView& vertex_buffer_view)
        : handle(vertex_buffer_view.GetGPUHandle())
        , size(vertex_buffer_view.GetSize())
        , stride(vertex_buffer_view.GetStride())
    
    {
        
    }
    
    RHIGPUDescriptorHandle handle;
    unsigned size;
    unsigned stride;
};
