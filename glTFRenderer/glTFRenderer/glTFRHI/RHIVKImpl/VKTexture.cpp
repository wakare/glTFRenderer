#include "VKTexture.h"

#include "VKGPUBuffer.h"

IRHIBuffer& VKTexture::GetGPUBuffer()
{
    static VKGPUBuffer temp;
    return temp;
}

bool VKTexture::InitTexture(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& desc)
{
    return false;
}
