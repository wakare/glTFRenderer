#pragma once
#include "DX12CommandAllocator.h"
#include "DX12Common.h"
#include "IRHICommandSignature.h"

class DX12CommandSignature : public IRHICommandSignature
{
public:
    DX12CommandSignature();
    
    virtual bool InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature) override;
    
    ID3D12CommandSignature* GetCommandSignature() const {return m_command_signature.Get(); }

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    ComPtr<ID3D12CommandSignature> m_command_signature; 
};
