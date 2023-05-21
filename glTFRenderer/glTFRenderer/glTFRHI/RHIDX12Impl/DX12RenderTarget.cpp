#include "DX12RenderTarget.h"

#include "DX12Utils.h"

DX12RenderTarget::DX12RenderTarget()
    : IRHIRenderTarget()
    , m_texture(nullptr)
    , m_clearValue({})
    , m_releaseInDtor(false)
{
}

DX12RenderTarget::~DX12RenderTarget()
{
    if (m_releaseInDtor)
    {
        SAFE_RELEASE(m_texture)    
    }
}

ID3D12Resource* DX12RenderTarget::GetResource() const
{
    return m_texture;
}
