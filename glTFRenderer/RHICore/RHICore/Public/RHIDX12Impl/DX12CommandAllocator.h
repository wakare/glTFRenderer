#pragma once
#include "DX12Common.h"
#include "IRHICommandAllocator.h"

class DX12CommandAllocator : public IRHICommandAllocator
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12CommandAllocator)
    
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) override;

    ID3D12CommandAllocator* GetCommandAllocator() const {return m_command_allocator.Get();}
    
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
private:
    ComPtr<ID3D12CommandAllocator> m_command_allocator;
};
