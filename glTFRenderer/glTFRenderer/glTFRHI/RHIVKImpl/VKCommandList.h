#pragma once
#include "VKCommandAllocator.h"
#include "glTFRHI/RHIInterface/IRHICommandList.h"

class VKCommandList : public IRHICommandList
{
public:
    VKCommandList() = default;
    virtual ~VKCommandList() override;
    DECLARE_NON_COPYABLE(VKCommandList)
    virtual bool InitCommandList(IRHIDevice& device, IRHICommandAllocator& command_allocator) override;

protected:
    VkCommandBuffer m_command_buffer {VK_NULL_HANDLE};
};
