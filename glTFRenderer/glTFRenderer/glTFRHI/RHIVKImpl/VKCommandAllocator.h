#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHICommandAllocator.h"

class VKCommandAllocator : public IRHICommandAllocator
{
public:
    VKCommandAllocator() = default;
    virtual ~VKCommandAllocator() override;
    DECLARE_NON_COPYABLE(VKCommandAllocator)
    
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) override;

    const VkCommandPool& GetCommandPool() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkCommandPool m_command_pool {VK_NULL_HANDLE};
};
