#include "IRHIBuffer.h"
#include "glTFRHI/RHIUtils.h"

bool IRHIBuffer::Transition(IRHICommandList& command_list, RHIResourceStateType new_state)
{
    if (m_current_state == new_state)
    {
        return true;
    }

    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *this, m_current_state, new_state))

    m_current_state = new_state;
    return true;
}

RHIResourceStateType IRHIBuffer::GetState() const
{
    return m_current_state;
}
