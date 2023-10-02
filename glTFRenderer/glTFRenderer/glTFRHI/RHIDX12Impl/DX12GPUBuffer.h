#pragma once

#include "../RHIInterface/IRHIGPUBuffer.h"
#include "DX12Common.h"

class DX12GPUBuffer : public IRHIGPUBuffer
{
public:
    DX12GPUBuffer();
    virtual ~DX12GPUBuffer() override;
    
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) override;
    virtual bool UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size) override;
    virtual GPU_BUFFER_HANDLE_TYPE GetGPUBufferHandle() override;
    
    ID3D12Resource* GetBuffer() const { return m_buffer.Get();}
    
private:
    ComPtr<ID3D12Resource> m_buffer;
    UINT8* m_mapped_gpu_buffer;
    CD3DX12_RANGE m_map_range;
};
