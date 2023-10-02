#pragma once
#include "DX12Common.h"

#include "../RHIInterface/IRHIRenderTarget.h"

class DX12RenderTarget : public IRHIRenderTarget
{
public:
    DX12RenderTarget();
    virtual ~DX12RenderTarget() override;

    void SetRenderTarget(ID3D12Resource* renderTarget)
    {
        assert(m_texture == nullptr);
        m_texture = renderTarget;
    }
    
    ID3D12Resource* GetRenderTarget() {return m_texture.Get();}

    void SetClearValue(D3D12_CLEAR_VALUE clear_value) {m_clearValue = clear_value; }
    const D3D12_CLEAR_VALUE& GetClearValue() const {return m_clearValue; }

    ID3D12Resource* GetResource() const;
    
protected:
    ComPtr<ID3D12Resource> m_texture;
    D3D12_CLEAR_VALUE m_clearValue;
};
