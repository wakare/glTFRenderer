#pragma once
#include "RHIInterface/IRHIMemoryAllocator.h"

class DX12MemoryAllocator : public IRHIMemoryAllocator
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12MemoryAllocator)
    
    virtual bool InitMemoryAllocator(const IRHIFactory& factory, const IRHIDevice& device) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
};
