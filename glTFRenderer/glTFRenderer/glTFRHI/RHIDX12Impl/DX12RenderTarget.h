#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIRenderTarget.h"

class DX12RenderTarget : public IRHIRenderTarget
{
public:
    DX12RenderTarget();

    ID3D12Resource* m_texture;
    D3D12_CLEAR_VALUE m_clearValue;
};
