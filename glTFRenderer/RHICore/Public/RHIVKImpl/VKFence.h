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
    virtual unsigned long long PredictNextSignalValue() const override;
    virtual void NotifySignalSubmitted(unsigned long long signal_value) override;
    virtual bool IsSignalValueCompleted(unsigned long long signal_value) const override;

    VkFence GetFence() const {return m_fence; }
    
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkFence m_fence {VK_NULL_HANDLE};
    unsigned long long m_last_submitted_signal_value{0};
    mutable unsigned long long m_last_completed_signal_value{0};
};
