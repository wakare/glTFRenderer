#include "RHIInterface/IRHIRenderTarget.h"

#include <utility>

#include "RHIInterface/IRHIDescriptorManager.h"

bool IRHIRenderTarget::InitRenderTarget(std::shared_ptr<IRHITextureAllocation> texture,
                                        std::shared_ptr<IRHITextureDescriptorAllocation> descriptor_allocation)
{
    m_texture_allocation = std::move(texture);
    m_descriptor_allocation = std::move(descriptor_allocation);
    
    return true;
}

std::shared_ptr<IRHITexture> IRHIRenderTarget::GetTexture() const
{
    return m_texture_allocation->m_texture;
}

std::shared_ptr<IRHITextureAllocation> IRHIRenderTarget::GetTextureAllocationSharedPtr() const
{
    return m_texture_allocation;
}

IRHITextureDescriptorAllocation& IRHIRenderTarget::GetDescriptorAllocation()
{
    return *m_descriptor_allocation;
}

RHIRenderTargetType IRHIRenderTarget::GetRenderTargetType() const
{
    return m_descriptor_allocation->GetDesc().m_view_type == RHIViewType::RVT_RTV ?
        RHIRenderTargetType::RTV : RHIRenderTargetType::DSV;
}

RHIDataFormat IRHIRenderTarget::GetRenderTargetFormat() const
{
    return m_descriptor_allocation->GetDesc().m_format;
}

RHITextureClearValue IRHIRenderTarget::GetClearValue() const
{
    return m_texture_allocation->m_texture->GetTextureDesc().GetClearValue();
}

bool IRHIRenderTarget::Transition(IRHICommandList& command_list, RHIResourceStateType new_state)
{
    return m_texture_allocation->m_texture->Transition(command_list, new_state);
}
