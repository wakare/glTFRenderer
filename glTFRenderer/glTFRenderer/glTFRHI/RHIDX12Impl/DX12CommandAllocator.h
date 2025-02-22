#pragma once
#include "DX12Common.h"
#include "glTFRHI/RHIInterface/IRHICommandAllocator.h"

class DX12CommandAllocator : public IRHICommandAllocator
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12CommandAllocator)
    
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) override;

    ID3D12CommandAllocator* GetCommandAllocator() const {return m_commandAllocator.Get();}
    
    virtual bool Release(glTFRenderResourceManager&) override;
    
private:
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
};
