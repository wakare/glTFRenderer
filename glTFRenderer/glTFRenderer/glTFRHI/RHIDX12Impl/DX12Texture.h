#pragma once
#include <memory>

#include "glTFRHI/RHIInterface/IRHIBuffer.h"
#include "glTFRHI/RHIInterface/IRHITexture.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

class DX12Texture : public IRHITexture
{
public:
    DX12Texture();
    ~DX12Texture() override;
    
    virtual bool UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath, bool srgb) override;
    virtual IRHIBuffer& GetGPUBuffer() override;
    
protected:
    std::shared_ptr<IRHIBufferAllocation> m_textureBuffer;
    std::shared_ptr<IRHIBufferAllocation> m_textureUploadBuffer;
};
