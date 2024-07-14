#include "IRHIDescriptorManager.h"

const RHIDescriptorDesc& IRHIBufferDescriptorAllocation::GetDesc() const
{
    return m_view_desc.value();
}

const RHIDescriptorDesc& IRHITextureDescriptorAllocation::GetDesc() const
{
    return m_view_desc.value();
}
