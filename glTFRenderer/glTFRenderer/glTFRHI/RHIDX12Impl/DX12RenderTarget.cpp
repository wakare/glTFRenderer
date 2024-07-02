#include "DX12RenderTarget.h"

#include "DX12Utils.h"

DX12RenderTarget::DX12RenderTarget()
{
}

DX12RenderTarget::~DX12RenderTarget()
{
    SAFE_RELEASE(m_texture)
}