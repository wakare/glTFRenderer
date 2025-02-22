#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHISubPass.h"

class VKSubPass : public IRHISubPass
{
public:
    VKSubPass();
    
    virtual bool InitSubPass(const RHISubPassInfo& sub_pass_info) override;
    virtual bool Release(glTFRenderResourceManager&) override;
    
    VkSubpassDescription GetSubPassDescription() const;
    
protected:
    VkSubpassDescription m_subpass_description;
};
