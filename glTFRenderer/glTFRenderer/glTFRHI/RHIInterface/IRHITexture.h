#pragma once

#include "IRHIDevice.h"
#include "IRHIResource.h"
#include <memory>

class IRHIGPUBuffer;
class IRHICommandList;
class glTFImageLoader;

struct RHITextureDesc
{
public:
    RHITextureDesc();
    RHITextureDesc(const RHITextureDesc&) = delete;
    RHITextureDesc(RHITextureDesc&&) = delete;

    RHITextureDesc& operator=(const RHITextureDesc&) = delete;
    RHITextureDesc& operator=(RHITextureDesc&&) = delete;
    
    bool Init(unsigned width, unsigned height, RHIDataFormat format);
    
    unsigned char* GetTextureData() const { return m_textureData.get(); }
    size_t GetTextureDataSize() const { return m_textureDataSize; }
    RHIDataFormat GetDataFormat() const { return m_textureFormat; }
    unsigned GetTextureWidth() const { return m_textureWidth; }
    unsigned GetTextureHeight() const { return m_textureHeight; }
    
private:
    std::unique_ptr<unsigned char[]> m_textureData;
    size_t m_textureDataSize;
    unsigned m_textureWidth;
    unsigned m_textureHeight;
    RHIDataFormat m_textureFormat;
};

class IRHITexture : public IRHIResource
{
public:
    virtual bool UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath) = 0;
    virtual IRHIGPUBuffer& GetGPUBuffer() = 0;
    
    const RHITextureDesc& GetTextureDesc() const {return m_textureDesc; }
    
protected:
    RHITextureDesc m_textureDesc = {};
};
