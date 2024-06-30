#pragma once
#include "DX12Common.h"
#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"

class DX12RenderTarget : public IRHIRenderTarget
{
public:
    DX12RenderTarget();
    virtual ~DX12RenderTarget() override;

    void SetRenderTarget(ID3D12Resource* renderTarget);
    void SetClearValue(D3D12_CLEAR_VALUE clear_value);
    
    const D3D12_CLEAR_VALUE& GetClearValue() const;
    ID3D12Resource* GetResource() const;
    
protected:
    ComPtr<ID3D12Resource> m_texture;
    D3D12_CLEAR_VALUE m_clearValue;
};