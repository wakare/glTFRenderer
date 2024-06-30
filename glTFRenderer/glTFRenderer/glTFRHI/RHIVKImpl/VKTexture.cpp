#include "VKTexture.h"

#include "VKGPUBuffer.h"

bool VKTexture::UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath,
                                      bool srgb)
{
    return true;
}

IRHIBuffer& VKTexture::GetGPUBuffer()
{
    static VKGPUBuffer temp;
    return temp;
}
