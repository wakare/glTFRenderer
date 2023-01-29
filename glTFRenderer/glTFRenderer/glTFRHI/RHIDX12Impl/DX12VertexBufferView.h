#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIVertexBufferView.h"

class DX12VertexBufferView : public IRHIVertexBufferView
{
public:
    DX12VertexBufferView();
    virtual void InitVertexBufferView(IRHIGPUBuffer& buffer, size_t offset, size_t vertexStride, size_t vertexBufferSize) override;

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const {return m_vertexBufferView; }
    
private:
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};
