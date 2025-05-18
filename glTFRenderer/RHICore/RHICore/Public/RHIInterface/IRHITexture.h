#pragma once

#include "IRHICommandList.h"
#include "IRHIResource.h"

class IRHIBuffer;
class IRHICommandList;
class glTFImageIOUtil;

class RHICORE_API IRHITexture : public IRHIResource
{
public:
    friend class IRHIMemoryManager;
    friend class DX12MemoryManager;
    
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHITexture)

    const RHITextureDesc& GetTextureDesc() const;
    RHIDataFormat GetTextureFormat() const;
    
    bool Transition(IRHICommandList& command_list, RHIResourceStateType new_state);
    RHIResourceStateType GetState() const;
    bool CanReadBack() const;
    
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    void SetCopyReq(RHIMipMapCopyRequirements copy_requirements);
    
    RHIMipMapCopyRequirements GetCopyReq() const;
    unsigned GetMipCount() const;
    
protected:
    RHITextureDesc m_texture_desc {};
    RHIMipMapCopyRequirements m_copy_requirements;
    
    // Default init texture with state common
    RHIResourceStateType m_current_state {RHIResourceStateType::STATE_UNDEFINED};
};
