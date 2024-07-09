#pragma once
#include <memory>
#include <d3d12.h>

#include "glTFRHI/RHIInterface/IRHITexture.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

class DX12Texture : public IRHITexture
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Texture)
    
    ID3D12Resource* GetRawResource() const;
    bool InitFromExternalResource(ID3D12Resource* raw_resource, const RHITextureDesc& desc);

    bool InitTexture(IRHIDevice& device, const RHITextureDesc& desc);
    bool InitTextureAndUpload(IRHIDevice& device,  IRHICommandList& command_list, const RHITextureDesc& desc);
    
protected:
    std::shared_ptr<IRHIBufferAllocation> m_texture_buffer;
    std::shared_ptr<IRHIBufferAllocation> m_texture_upload_buffer;

    // Only valid when real resource creation is from external
    ID3D12Resource* m_raw_resource {nullptr};
};
