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

    bool InitRenderTarget(std::shared_ptr<IRHITextureAllocation> texture,
                          std::shared_ptr<IRHIDescriptorAllocation> descriptor_allocation);

    IRHITexture& GetTexture() const;
    IRHITextureAllocation& GetTextureAllocation() const;
    std::shared_ptr<IRHITextureAllocation> GetTextureAllocationSharedPtr() const;
    IRHIDescriptorAllocation& GetDescriptorAllocation();
    
    RHIRenderTargetType GetRenderTargetType() const { return m_descriptor_allocation->m_view_desc.view_type == RHIViewType::RVT_RTV ?
        RHIRenderTargetType::RTV : RHIRenderTargetType::DSV; }
    RHIDataFormat GetRenderTargetFormat() const { return m_descriptor_allocation->m_view_desc.format; }
    RHITextureClearValue GetClearValue() const {return m_texture_allocation->m_texture->GetTextureDesc().GetClearValue(); }

    bool Transition(IRHICommandList& command_list, RHIResourceStateType new_state);
    
private:
    std::shared_ptr<IRHITextureAllocation> m_texture_allocation;
    std::shared_ptr<IRHIDescriptorAllocation> m_descriptor_allocation;
};