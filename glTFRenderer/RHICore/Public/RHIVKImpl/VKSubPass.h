#pragma once
#include "VolkUtils.h"
#include "RHIInterface/IRHISubPass.h"

class VKSubPass : public IRHISubPass
{
public:
    VKSubPass();
    
    virtual bool InitSubPass(const RHISubPassInfo& sub_pass_info) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    VkSubpassDescription GetSubPassDescription() const;
    
protected:
    VkSubpassDescription m_subpass_description;
};
