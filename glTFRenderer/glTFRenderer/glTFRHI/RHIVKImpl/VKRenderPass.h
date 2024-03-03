#pragma once
#include <vulkan/vulkan_core.h>
#include "glTFRHI/RHIInterface/IRHIRenderPass.h"

class VKRenderPass : public IRHIRenderPass
{
public:
    virtual bool InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info) override;

    const VkRenderPass& GetRenderPass() const;
    
protected:
    VkRenderPass m_render_pass;
};
