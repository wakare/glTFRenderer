#pragma once
#include "VolkUtils.h"
#include "IRHIRenderPass.h"

class VKRenderPass : public IRHIRenderPass
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKRenderPass)
    
    virtual bool InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    const VkRenderPass& GetRenderPass() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkRenderPass m_render_pass {VK_NULL_HANDLE};
};
