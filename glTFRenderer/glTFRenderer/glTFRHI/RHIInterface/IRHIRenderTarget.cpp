#include "IRHIRenderTarget.h"

#include <utility>

IRHIRenderTarget::IRHIRenderTarget()
{
}

bool IRHIRenderTarget::InitRenderTarget(std::shared_ptr<IRHITexture> texture,
    std::shared_ptr<IRHIDescriptorAllocation> descriptor_allocation)
{
    m_texture = std::move(texture);
    m_descriptor_allocation = std::move(descriptor_allocation);
    
    return true;
}

IRHITexture& IRHIRenderTarget::GetTexture() const
{
    return *m_texture;
}

IRHIDescriptorAllocation& IRHIRenderTarget::GetDescriptorAllocation()
{
    return *m_descriptor_allocation;
}
