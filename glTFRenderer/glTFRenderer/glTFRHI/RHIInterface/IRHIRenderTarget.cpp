#include "IRHIRenderTarget.h"

#include <utility>

IRHIRenderTarget::IRHIRenderTarget()
= default;

bool IRHIRenderTarget::InitRenderTarget(std::shared_ptr<IRHITextureAllocation> texture,
                                        std::shared_ptr<IRHIDescriptorAllocation> descriptor_allocation)
{
    m_texture_allocation = std::move(texture);
    m_descriptor_allocation = std::move(descriptor_allocation);
    
    return true;
}

IRHITexture& IRHIRenderTarget::GetTexture() const
{
    return *m_texture_allocation->m_texture;
}

std::shared_ptr<IRHITextureAllocation> IRHIRenderTarget::GetTextureAllocationSharedPtr() const
{
    return m_texture_allocation;
}

IRHIDescriptorAllocation& IRHIRenderTarget::GetDescriptorAllocation()
{
    return *m_descriptor_allocation;
}

bool IRHIRenderTarget::Transition(IRHICommandList& command_list, RHIResourceStateType new_state)
{
    return m_texture_allocation->m_texture->Transition(command_list, new_state);
}
