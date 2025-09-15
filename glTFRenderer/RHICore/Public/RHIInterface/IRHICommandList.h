#pragma once
#include "RHIInterface/IRHIResource.h"

class IRHIDevice;
class IRHIFence;
class IRHISemaphore;
class IRHICommandAllocator;

enum class RHICommandListState
{
    Recording,
    Closed
};

class RHICORE_API IRHICommandList : public IRHIResource
{
public:
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& command_allocator) = 0;
    virtual bool WaitCommandList() = 0;
    virtual bool BeginRecordCommandList() = 0;
    virtual bool EndRecordCommandList() = 0;

    IRHISemaphore& GetSemaphore() const;
    IRHIFence& GetFence() const;

    void SetState(RHICommandListState state);
    RHICommandListState GetState() const;
    
protected:
    std::shared_ptr<IRHISemaphore> m_finished_semaphore;
    std::shared_ptr<IRHIFence> m_fence;

    RHICommandListState m_state {RHICommandListState::Closed};
};
