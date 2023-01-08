#pragma once
#include <d3d12.h>
#include "../RHIInterface/IRHICommandAllocator.h"

class DX12CommandAllocator : public IRHICommandAllocator
{
public:
    DX12CommandAllocator();
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) override;

    ID3D12CommandAllocator* GetCommandAllocator() const {return m_commandAllocator;}
    
private:
    ID3D12CommandAllocator* m_commandAllocator;
};
