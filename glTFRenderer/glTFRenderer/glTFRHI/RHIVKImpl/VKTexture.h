#pragma once
#include "glTFRHI/RHIInterface/IRHITexture.h"

class VKTexture : public IRHITexture
{
public:
    virtual bool UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath, bool srgb) override;
    virtual IRHIGPUBuffer& GetGPUBuffer() override;
};
