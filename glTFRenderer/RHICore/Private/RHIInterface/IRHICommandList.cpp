#include "RHIInterface/IRHICommandList.h"

IRHISemaphore& IRHICommandList::GetSemaphore() const
{
    return *m_finished_semaphore;
}

IRHIFence& IRHICommandList::GetFence() const
{
    return *m_fence;
}

void IRHICommandList::SetState(RHICommandListState state)
{
    m_state = state;
}

RHICommandListState IRHICommandList::GetState() const
{
    return m_state;
}
