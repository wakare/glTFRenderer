#pragma once
#include <d3d12.h>

#include "glTFRHI/RHIInterface/IRHIVertexBufferView.h"

class DX12VertexBufferView : public IRHIVertexBufferView
{
public:
    DX12VertexBufferView();
    virtual ~DX12VertexBufferView() override;
    
    virtual void InitVertexBufferView(IRHIGPUBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) override;

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const {return m_vertexBufferView; }
    
    virtual RHIGPUDescriptorHandle GetGPUHandle() const override;
    virtual size_t GetSize() const override;
    virtual size_t GetStride() const override;
    
private:
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};
