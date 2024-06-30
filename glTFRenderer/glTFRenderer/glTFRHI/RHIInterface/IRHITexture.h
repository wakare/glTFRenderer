#pragma once

#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIBuffer;
class IRHICommandList;
class glTFImageLoader;

class IRHITexture : public IRHIResource
{
public:
    friend class IRHIMemoryManager;
    friend class DX12MemoryManager;
    
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHITexture)

    virtual IRHIBuffer& GetGPUBuffer() = 0;
    const RHITextureDesc& GetTextureDesc() const {return m_texture_desc; }
    
protected:
    virtual bool InitTexture(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& desc) = 0;

    RHITextureDesc m_texture_desc {};
};
