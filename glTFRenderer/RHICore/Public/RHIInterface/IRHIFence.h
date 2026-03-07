#pragma once
#include "RHIInterface/IRHIResource.h"

class IRHIDevice;

class RHICORE_API IRHIFence : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIFence)
    
    virtual bool InitFence(IRHIDevice& device) = 0;
    virtual bool HostWaitUtilSignaled() = 0;
    virtual bool ResetFence() = 0;
    virtual unsigned long long PredictNextSignalValue() const = 0;
    virtual void NotifySignalSubmitted(unsigned long long signal_value) = 0;
    virtual bool IsSignalValueCompleted(unsigned long long signal_value) const = 0;

    void SetCanWait(bool enable);
    bool CanWait() const;

protected:
    bool m_can_wait{true};
};
