#pragma once
#include "IRHICommandAllocator.h"
#include "IRHIResource.h"

class IRHIFence;
class IRHISemaphore;

class IRHICommandList : public IRHIResource
{
public:
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& command_allocator) = 0;
    virtual bool WaitCommandList() = 0;
    virtual bool BeginRecordCommandList() = 0;
    virtual bool EndRecordCommandList() = 0;

    IRHISemaphore& GetSemaphore() const {return *m_finished_semaphore; }
    IRHIFence& GetFence() const {return *m_fence; }
    
protected:
    std::shared_ptr<IRHISemaphore> m_finished_semaphore;
    std::shared_ptr<IRHIFence> m_fence;
};
