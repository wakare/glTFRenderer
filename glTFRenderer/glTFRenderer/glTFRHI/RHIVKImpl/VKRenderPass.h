#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderPass.h"

class VKRenderPass : public IRHIRenderPass
{
public:
    VKRenderPass() = default;
    virtual ~VKRenderPass() override;
    DECLARE_NON_COPYABLE(VKRenderPass)
    
    virtual bool InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info) override;

    const VkRenderPass& GetRenderPass() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkRenderPass m_render_pass {VK_NULL_HANDLE};
};
