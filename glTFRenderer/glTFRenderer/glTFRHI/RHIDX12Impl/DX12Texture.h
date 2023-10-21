#pragma once
#include <memory>

#include "../RHIInterface/IRHIGPUBuffer.h"
#include "../RHIInterface/IRHITexture.h"

class DX12Texture : public IRHITexture
{
public:
    DX12Texture();
    ~DX12Texture() override;
    
    virtual bool UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath, bool srgb) override;
    virtual IRHIGPUBuffer& GetGPUBuffer() override;
    
protected:
    std::shared_ptr<IRHIGPUBuffer> m_textureBuffer;
    std::shared_ptr<IRHIGPUBuffer> m_textureUploadBuffer;
};
