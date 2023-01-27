#pragma once

#include "../RHIInterface/IRHIGPUBuffer.h"
#include "../RHIInterface/IRHIGPUBufferManager.h"
#include <d3d12.h>

class DX12GPUBuffer : public IRHIGPUBuffer
{
public:
    DX12GPUBuffer();
    
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) override;
    virtual bool UploadBufferFromCPU(void* data, size_t size) override;
    
    ID3D12Resource* GetBuffer() {return m_buffer;}
    
private:
    ID3D12Resource* m_buffer;
    UINT8* m_mappedGPUBuffer;
    RHIBufferDesc m_bufferDesc;
};
