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

    const RHITextureDesc& GetTextureDesc() const {return m_texture_desc; }
    RHIDataFormat GetTextureFormat() const {return GetTextureDesc().GetDataFormat(); }
    
    bool Transition(IRHICommandList& command_list, RHIResourceStateType new_state);
    RHIResourceStateType GetState() const;
    bool CanReadBack() const;
    
    virtual bool Release(glTFRenderResourceManager&) override;
    
protected:
    RHITextureDesc m_texture_desc {};

    // Default init texture with state common
    RHIResourceStateType m_current_state {RHIResourceStateType::STATE_UNDEFINED};
};
