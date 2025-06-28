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

    void SetCanWait(bool enable);
    bool CanWait() const;

protected:
    bool m_can_wait{true};
};
