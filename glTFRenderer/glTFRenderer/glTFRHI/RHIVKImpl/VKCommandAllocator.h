#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHICommandAllocator.h"

class VKCommandAllocator : public IRHICommandAllocator
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKCommandAllocator)
    
    virtual bool InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type) override;

    const VkCommandPool& GetCommandPool() const;

    bool Release(glTFRenderResourceManager&) override;
    
protected:
    VkCommandPool m_command_pool {VK_NULL_HANDLE};
};
