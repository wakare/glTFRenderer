#pragma once
#include <atomic>

#include "IRHIMemoryManager.h"
#include "IRHIResource.h"
#include "IRHITexture.h"

class IRHIRenderTargetManager;

struct RHIRenderTargetDesc
{
    RHIRenderTargetType type;
    RHIDataFormat format;
};

class RHICORE_API IRHIRenderTarget
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRenderTarget)

    bool InitRenderTarget(std::shared_ptr<IRHITextureAllocation> texture,
                          std::shared_ptr<IRHITextureDescriptorAllocation> descriptor_allocation);

    std::shared_ptr<IRHITexture> GetTexture() const;
    std::shared_ptr<IRHITextureAllocation> GetTextureAllocationSharedPtr() const;
    IRHITextureDescriptorAllocation& GetDescriptorAllocation();
    
    RHIRenderTargetType GetRenderTargetType() const ;
    RHIDataFormat GetRenderTargetFormat() const;
    RHITextureClearValue GetClearValue() const;

    bool Transition(IRHICommandList& command_list, RHIResourceStateType new_state);
    
private:
    std::shared_ptr<IRHITextureAllocation> m_texture_allocation;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_descriptor_allocation;
};