#include "DX12RenderTarget.h"

#include "DX12Utils.h"

DX12RenderTarget::DX12RenderTarget()
    : m_texture(nullptr)
    , m_clearValue({})
{
}

DX12RenderTarget::~DX12RenderTarget()
{
    SAFE_RELEASE(m_texture)
}

void DX12RenderTarget::SetRenderTarget(ID3D12Resource* renderTarget)
{
    assert(m_texture == nullptr);
    m_texture = renderTarget;
}

void DX12RenderTarget::SetClearValue(D3D12_CLEAR_VALUE clear_value)
{
    m_clearValue = clear_value;
}

const D3D12_CLEAR_VALUE& DX12RenderTarget::GetClearValue() const
{
    return m_clearValue;
}

ID3D12Resource* DX12RenderTarget::GetResource() const
{
    return m_texture.Get();
}
