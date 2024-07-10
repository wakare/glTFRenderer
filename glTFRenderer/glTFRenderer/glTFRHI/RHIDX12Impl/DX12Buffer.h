#pragma once

#include "glTFRHI/RHIInterface/IRHIBuffer.h"
#include "DX12Common.h"

class DX12Buffer : public IRHIBuffer
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR(DX12Buffer)
    virtual ~DX12Buffer() override;
    
    GPU_BUFFER_HANDLE_TYPE GetGPUBufferHandle() const;
    
    ID3D12Resource* GetBuffer() const { return m_buffer.Get();}
    bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc);
    bool UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size);
    
private:
    ComPtr<ID3D12Resource> m_buffer {nullptr};
    UINT8* m_mapped_gpu_buffer {nullptr};
    CD3DX12_RANGE m_map_range {};
};
