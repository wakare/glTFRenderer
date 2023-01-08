#include "DX12RenderTarget.h"

DX12RenderTarget::DX12RenderTarget()
    : IRHIRenderTarget()
    , m_texture(nullptr)
    , m_clearValue({})
{
}
