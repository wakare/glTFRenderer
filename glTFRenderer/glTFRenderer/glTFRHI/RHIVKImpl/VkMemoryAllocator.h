#pragma once
#include <vma/vk_mem_alloc.h>
#include "glTFRHI/RHIInterface/IRHIMemoryAllocator.h"

class VkMemoryAllocator : public IRHIMemoryAllocator
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VkMemoryAllocator)
    virtual bool InitMemoryAllocator(const IRHIFactory& factory, const IRHIDevice& device) override;

protected:
    VmaAllocator m_vma_allocator;
};