#pragma once
#include "VolkUtils.h"
#include "IRHICommandList.h"

class IRHIFence;

class VKCommandList : public IRHICommandList
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKCommandList)
    
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& command_allocator) override;
    virtual bool WaitCommandList() override;

    virtual bool BeginRecordCommandList() override;
    virtual bool EndRecordCommandList() override;
    
    VkCommandBuffer GetRawCommandBuffer() const {return m_command_buffer; }

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    VkCommandBuffer m_command_buffer {VK_NULL_HANDLE};
};
