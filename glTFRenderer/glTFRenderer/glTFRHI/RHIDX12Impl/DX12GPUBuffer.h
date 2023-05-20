#pragma once

#include "../RHIInterface/IRHIGPUBuffer.h"
#include "../RHIInterface/IRHIGPUBufferManager.h"
#include <d3d12.h>

#include "d3dx12.h"

class DX12GPUBuffer : public IRHIGPUBuffer
{
public:
    DX12GPUBuffer();
    virtual ~DX12GPUBuffer() override;
    
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) override;
    virtual bool UploadBufferFromCPU(void* data, size_t dataOffset, size_t size) override;
    virtual GPU_BUFFER_HANDLE_TYPE GetGPUBufferHandle() override;
    
    ID3D12Resource* GetBuffer() const {return m_buffer;}
    
private:
    ID3D12Resource* m_buffer;
    UINT8* m_mappedGPUBuffer;
    
    RHIBufferDesc m_bufferDesc;
    CD3DX12_RANGE m_mapRange;
};
