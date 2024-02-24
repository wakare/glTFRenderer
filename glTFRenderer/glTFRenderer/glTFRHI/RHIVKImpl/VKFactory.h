#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIFactory.h"

class VKFactory : public IRHIFactory
{
public:
    VKFactory();
    virtual ~VKFactory() override;
    
    virtual bool InitFactory() override;
    
    VkInstance GetInstance() const {return m_instance; }
    
protected:
    VkInstance m_instance;
};
