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
    
    unsigned char* GetTextureData() const { return m_texture_data.get(); }
    size_t GetTextureDataSize() const { return m_texture_data_size; }
    RHIDataFormat GetDataFormat() const { return m_texture_format; }
    unsigned GetTextureWidth() const { return m_texture_width; }
    unsigned GetTextureHeight() const { return m_texture_height; }

    void SetDataFormat(RHIDataFormat format) { m_texture_format = format;}
    
private:
    std::unique_ptr<unsigned char[]> m_texture_data;
    size_t m_texture_data_size;
    unsigned m_texture_width;
    unsigned m_texture_height;
    RHIDataFormat m_texture_format;
};

class IRHITexture : public IRHIResource
{
public:
    virtual bool UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath, bool srgb) = 0;
    virtual IRHIGPUBuffer& GetGPUBuffer() = 0;
    
    const RHITextureDesc& GetTextureDesc() const {return m_textureDesc; }
    
protected:
    RHITextureDesc m_textureDesc = {};
};
