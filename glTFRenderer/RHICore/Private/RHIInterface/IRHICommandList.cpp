#include "RHIInterface/IRHICommandList.h"

IRHISemaphore& IRHICommandList::GetSemaphore() const
{
    return *m_finished_semaphore;
}

IRHIFence& IRHICommandList::GetFence() const
{
    return *m_fence;
}

std::shared_ptr<IRHIFence> IRHICommandList::GetFenceSharedPtr() const
{
    return m_fence;
}

void IRHICommandList::SetState(RHICommandListState state)
{
    m_state = state;
}

RHICommandListState IRHICommandList::GetState() const
{
    return m_state;
}

void IRHICommandList::SetFrameSlotIndex(unsigned frame_slot_index)
{
    m_frame_slot_index = frame_slot_index;
}

unsigned IRHICommandList::GetFrameSlotIndex() const
{
    return m_frame_slot_index;
}
