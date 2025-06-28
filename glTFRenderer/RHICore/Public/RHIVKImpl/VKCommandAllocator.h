#pragma once
#include "VolkUtils.h"
#include "RHIInterface/IRHICommandAllocator.h"

class RHICORE_API VKCommandAllocator : public IRHICommandAllocator
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKCommandAllocator)
    
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) override;

    const VkCommandPool& GetCommandPool() const;

    bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    VkCommandPool m_command_pool {VK_NULL_HANDLE};
    VkDevice m_device {VK_NULL_HANDLE};
};
