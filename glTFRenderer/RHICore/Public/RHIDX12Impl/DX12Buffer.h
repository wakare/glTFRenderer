#pragma once

#include "RHIInterface/IRHIBuffer.h"
#include "DX12Common.h"
#include "RHIUtils.h"

class DX12Buffer : public IRHIBuffer
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Buffer)
    
    bool Release(IRHIMemoryManager& memory_manager) override;
    
    GPU_BUFFER_HANDLE_TYPE GetGPUBufferHandle() const;
    
    ID3D12Resource* GetRawBuffer() const { GLTF_CHECK(m_buffer.Get()); return m_buffer.Get();}
    bool CreateBuffer(IRHIDevice& device, const RHIBufferDesc& desc);
    bool UploadBufferFromCPU(const void* data, size_t dst_offset, size_t size);
    bool DownloadBufferToCPU(void* data, size_t size);

    RHIMipMapCopyRequirements GetMipMapRequirements() const;
    
private:
    ComPtr<ID3D12Resource> m_buffer {nullptr};
    UINT8* m_mapped_gpu_buffer {nullptr};
    CD3DX12_RANGE m_map_range {};
    
    RHIMipMapCopyRequirements m_copy_req;
};
