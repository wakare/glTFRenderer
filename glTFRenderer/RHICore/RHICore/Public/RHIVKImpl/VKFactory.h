#pragma once
#include "VolkUtils.h"

#include "IRHIFactory.h"

class VKFactory : public IRHIFactory
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKFactory)
    
    virtual bool InitFactory() override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    VkInstance GetInstance() const {return m_instance; }
    
protected:
    VkInstance m_instance;
};
