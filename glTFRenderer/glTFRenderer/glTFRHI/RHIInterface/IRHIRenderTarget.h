#pragma once
#include <atomic>

#include "IRHIMemoryManager.h"
#include "IRHIResource.h"
#include "IRHITexture.h"

class IRHIRenderTargetManager;

class IRHIRenderTarget : public IRHIResource
{
public:
    IRHIRenderTarget();
    virtual ~IRHIRenderTarget() override = default;
    DECLARE_NON_COPYABLE(IRHIRenderTarget)

    bool InitRenderTarget(std::shared_ptr<IRHITexture> texture,
        std::shared_ptr<IRHIDescriptorAllocation> descriptor_allocation);

    IRHITexture& GetTexture() const;
    IRHIDescriptorAllocation& GetDescriptorAllocation();
    
    RHIRenderTargetType GetRenderTargetType() const { return m_descriptor_allocation->m_view_desc.view_type == RHIViewType::RVT_RTV ?
        RHIRenderTargetType::RTV : RHIRenderTargetType::DSV; }
    RHIDataFormat GetRenderTargetFormat() const { return m_descriptor_allocation->m_view_desc.format; }
    RHITextureClearValue GetClearValue() const {return m_texture->GetTextureDesc().GetClearValue(); }
    
private:
    std::shared_ptr<IRHITexture> m_texture;
    std::shared_ptr<IRHIDescriptorAllocation> m_descriptor_allocation;
};