#pragma once
#include <memory>

#include "glTFRHI/RHIInterface/IRHIBuffer.h"
#include "glTFRHI/RHIInterface/IRHITexture.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

class DX12Texture : public IRHITexture
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Texture)

    virtual IRHIBuffer& GetGPUBuffer() override;
    
protected:
    virtual bool InitTexture(IRHIDevice& device,  IRHICommandList& command_list, const RHITextureDesc& desc) override;
    
    std::shared_ptr<IRHIBufferAllocation> m_texture_buffer;
    std::shared_ptr<IRHIBufferAllocation> m_texture_upload_buffer;
};
