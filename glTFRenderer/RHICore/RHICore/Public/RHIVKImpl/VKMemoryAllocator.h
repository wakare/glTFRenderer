#pragma once
#include <vma/vk_mem_alloc.h>
#include "IRHIMemoryAllocator.h"

class VKMemoryAllocator : public IRHIMemoryAllocator
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKMemoryAllocator)
    virtual bool InitMemoryAllocator(const IRHIFactory& factory, const IRHIDevice& device) override;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    VmaAllocator GetAllocator() const;

protected:
    VmaAllocator m_vma_allocator;
};
