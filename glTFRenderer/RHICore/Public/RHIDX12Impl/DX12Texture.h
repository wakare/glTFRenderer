#pragma once
#include <memory>
#include "DX12Common.h" 
#include "RHIInterface/IRHITexture.h"
#include "RHIInterface/IRHIMemoryManager.h"

class DX12Texture : public IRHITexture
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Texture)

    bool InitFromExternalResource(ID3D12Resource* raw_resource, const RHITextureDesc& desc);

    bool InitTexture(std::shared_ptr<IRHIBufferAllocation> buffer, const RHITextureDesc& desc);
    
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    ID3D12Resource* GetRawResource();
    const ID3D12Resource* GetRawResource() const;
    std::shared_ptr<IRHIBufferAllocation> GetTextureAllocation() const;
    
protected:
    std::shared_ptr<IRHIBufferAllocation> m_texture_buffer;

    // Only valid when real resource creation is from external
    ComPtr<ID3D12Resource> m_raw_resource {nullptr};
};
