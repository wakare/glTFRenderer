#pragma once
#include "VKCommandAllocator.h"
#include "glTFRHI/RHIInterface/IRHICommandList.h"
#include "glTFRHI/RHIInterface/IRHISemaphore.h"

class IRHIFence;

class VKCommandList : public IRHICommandList
{
public:
    VKCommandList() = default;
    virtual ~VKCommandList() override;
    DECLARE_NON_COPYABLE(VKCommandList)
    
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& command_allocator) override;
    virtual bool WaitCommandList() override;

    virtual bool BeginRecordCommandList() override;
    virtual bool EndRecordCommandList() override;
    
    VkCommandBuffer GetRawCommandBuffer() const {return m_command_buffer; }
    
protected:
    VkCommandBuffer m_command_buffer {VK_NULL_HANDLE};
};
