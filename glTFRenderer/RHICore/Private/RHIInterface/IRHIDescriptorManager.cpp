#include "RHIInterface/IRHIDescriptorManager.h"

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

void IRHIDescriptorManager::SetFrameSlotCount(unsigned frame_slot_count)
{
    m_frame_slot_count = frame_slot_count > 0 ? frame_slot_count : 1u;
}

unsigned IRHIDescriptorManager::GetFrameSlotCount() const
{
    return m_frame_slot_count;
}
