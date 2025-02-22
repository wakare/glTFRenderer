#include "IRHIDescriptorManager.h"

const RHIDescriptorDesc& IRHIBufferDescriptorAllocation::GetDesc() const
{
    return m_view_desc.value();
}

bool IRHIDescriptorAllocation::Release(glTFRenderResourceManager&)
{
    return true;
}

const RHIDescriptorDesc& IRHITextureDescriptorAllocation::GetDesc() const
{
    return m_view_desc.value();
}

bool IRHIDescriptorTable::Release(glTFRenderResourceManager&)
{
    return true;
}
