#pragma once
#include "VKDevice.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"

class VKFence : public IRHIFence
{
public:
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool HostWaitUtilSignaled() override;
    virtual bool ResetFence() override;

    VkFence GetFence() const {return m_fence; }
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkFence m_fence {VK_NULL_HANDLE};
};
