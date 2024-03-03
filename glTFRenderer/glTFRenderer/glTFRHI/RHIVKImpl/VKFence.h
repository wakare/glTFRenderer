#pragma once
#include "VKDevice.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"

class VKFence : public IRHIFence
{
public:
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool HostWaitUtilSignaled() override;

protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkFence m_fence {VK_NULL_HANDLE};
};
