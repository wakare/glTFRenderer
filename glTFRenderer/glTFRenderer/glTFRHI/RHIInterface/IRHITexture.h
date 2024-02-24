#pragma once

#include "IRHIDevice.h"
#include "IRHIResource.h"
#include <memory>

class IRHIGPUBuffer;
class IRHICommandList;
class glTFImageLoader;

class IRHITexture : public IRHIResource
{
public:
    virtual bool UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath, bool srgb) = 0;
    virtual IRHIGPUBuffer& GetGPUBuffer() = 0;
    
    const RHITextureDesc& GetTextureDesc() const {return m_textureDesc; }
    
protected:
    RHITextureDesc m_textureDesc = {};
};
