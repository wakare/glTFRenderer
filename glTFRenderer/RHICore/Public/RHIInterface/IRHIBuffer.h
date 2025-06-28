#pragma once
#include "RHIInterface/IRHICommandList.h"
#include "RHIInterface/IRHIResource.h"

class RHICORE_API IRHIBuffer : public IRHIResource
{
    friend class IRHIMemoryManager;
    friend class DX12MemoryManager;
    
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIBuffer)
    
    const RHIBufferDesc& GetBufferDesc() const {return m_buffer_desc; }
    
    bool Transition(IRHICommandList& command_list, RHIResourceStateType new_state);
    RHIResourceStateType GetState() const;

    bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    RHIBufferDesc m_buffer_desc {};

    // Default init buffer with state common
    RHIResourceStateType m_current_state {RHIResourceStateType::STATE_COMMON};
};