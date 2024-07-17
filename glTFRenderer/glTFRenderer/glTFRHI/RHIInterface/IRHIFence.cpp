#include "IRHIFence.h"

void IRHIFence::SetCanWait(bool enable)
{
    m_can_wait = enable;
}

bool IRHIFence::CanWait() const
{
    return m_can_wait;
}
