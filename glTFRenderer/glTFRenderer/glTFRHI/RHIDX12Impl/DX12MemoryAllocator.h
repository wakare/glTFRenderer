#pragma once
#include "glTFRHI/RHIInterface/IRHIMemoryAllocator.h"

class DX12MemoryAllocator : public IRHIMemoryAllocator
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12MemoryAllocator)
    
    virtual bool InitMemoryAllocator(const IRHIFactory& factory, const IRHIDevice& device) override;
    virtual bool Release(glTFRenderResourceManager&) override;
    
};
