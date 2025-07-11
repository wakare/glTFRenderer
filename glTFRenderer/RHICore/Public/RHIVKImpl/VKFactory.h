#pragma once
#include "VolkUtils.h"

#include "RHIInterface/IRHIFactory.h"

class RHICORE_API VKFactory : public IRHIFactory
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKFactory)
    
    virtual bool InitFactory() override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    VkInstance GetInstance() const {return m_instance; }
    
protected:
    VkInstance m_instance;
};
