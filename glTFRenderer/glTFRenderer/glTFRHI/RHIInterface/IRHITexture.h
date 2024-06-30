#pragma once

#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIBuffer;
class IRHICommandList;
class glTFImageLoader;

class IRHITexture : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHITexture)
    
    virtual bool UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath, bool srgb) = 0;
    virtual IRHIBuffer& GetGPUBuffer() = 0;
    
    const RHITextureDesc& GetTextureDesc() const {return m_textureDesc; }
    
protected:
    RHITextureDesc m_textureDesc = {};
};
