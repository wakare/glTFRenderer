#include "IRHIDescriptorManager.h"

const RHIDescriptorDesc& IRHIBufferDescriptorAllocation::GetDesc() const
{
    return m_view_desc.value();
}

bool IRHIDescriptorAllocation::Release(IRHIMemoryManager& memory_manager)
{
    return true;
}

const RHIDescriptorDesc& IRHITextureDescriptorAllocation::GetDesc() const
{
    return m_view_desc.value();
}

bool IRHIDescriptorTable::Release(IRHIMemoryManager& memory_manager)
{
    return true;
}
