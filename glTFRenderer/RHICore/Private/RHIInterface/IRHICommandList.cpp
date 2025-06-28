#include "RHIInterface/IRHICommandList.h"

IRHISemaphore& IRHICommandList::GetSemaphore() const
{
    return *m_finished_semaphore;
}

IRHIFence& IRHICommandList::GetFence() const
{
    return *m_fence;
}
