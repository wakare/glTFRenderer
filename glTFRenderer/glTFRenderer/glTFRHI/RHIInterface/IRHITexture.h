#pragma once

#include "IRHICommandList.h"
#include "IRHIResource.h"

class IRHIBuffer;
class IRHICommandList;
class glTFImageIOUtil;

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
    void SetCopyReq(RHIMipMapCopyRequirements copy_requirements);
    
    RHIMipMapCopyRequirements GetCopyReq() const;
    unsigned GetMipCount() const;
    
protected:
    RHITextureDesc m_texture_desc {};
    RHIMipMapCopyRequirements m_copy_requirements;
    
    // Default init texture with state common
    RHIResourceStateType m_current_state {RHIResourceStateType::STATE_UNDEFINED};
};
