#pragma once
#include "VKDevice.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"

class VKFence : public IRHIFence
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKFence)
    
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool HostWaitUtilSignaled() override;
    virtual bool ResetFence() override;

    VkFence GetFence() const {return m_fence; }
    
    virtual bool Release(glTFRenderResourceManager&) override;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkFence m_fence {VK_NULL_HANDLE};
};
