#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIRenderTarget.h"

class DX12RenderTarget : public IRHIRenderTarget
{
public:
    DX12RenderTarget();
    virtual ~DX12RenderTarget() override;

    void SetRenderTarget(ID3D12Resource* renderTarget, bool releaseInDtor)
    {
        assert(m_texture == nullptr);
        m_texture = renderTarget;
        m_releaseInDtor = releaseInDtor;
    }
    
    ID3D12Resource* GetRenderTarget() {return m_texture;}

    void SetClearValue(D3D12_CLEAR_VALUE clearValue) {m_clearValue = clearValue; }
    const D3D12_CLEAR_VALUE& GetClearValue() const {return m_clearValue; }
    
protected:
    ID3D12Resource* m_texture;
    D3D12_CLEAR_VALUE m_clearValue;
    bool m_releaseInDtor;
};
