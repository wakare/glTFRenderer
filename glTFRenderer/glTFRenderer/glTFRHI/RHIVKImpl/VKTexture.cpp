#include "VKTexture.h"

#include "VKGPUBuffer.h"

bool VKTexture::UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath,
                                      bool srgb)
{
    return true;
}

IRHIGPUBuffer& VKTexture::GetGPUBuffer()
{
    static VKGPUBuffer temp;
    return temp;
}
