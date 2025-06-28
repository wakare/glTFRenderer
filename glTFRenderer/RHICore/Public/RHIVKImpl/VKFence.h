#pragma once
#include "VKDevice.h"
#include "RHIInterface/IRHIFence.h"

class RHICORE_API VKFence : public IRHIFence
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKFence)
    
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool HostWaitUtilSignaled() override;
    virtual bool ResetFence() override;

    VkFence GetFence() const {return m_fence; }
    
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkFence m_fence {VK_NULL_HANDLE};
};
