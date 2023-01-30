#include "DX12RenderTarget.h"

#include "DX12Utils.h"

DX12RenderTarget::DX12RenderTarget()
    : IRHIRenderTarget()
    , m_texture(nullptr)
    , m_clearValue({})
{
}

DX12RenderTarget::~DX12RenderTarget()
{
    SAFE_RELEASE(m_texture)
}
