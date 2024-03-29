#pragma once
#include "DX12Common.h"
#include <memory>
#include <vector>

#include "DX12GPUBuffer.h"
#include "../RHIInterface/IRHIGPUBufferManager.h"

class DX12GPUBufferManager : public IRHIGPUBufferManager
{
public:
    DX12GPUBufferManager();
    virtual ~DX12GPUBufferManager() override;
    
    virtual bool InitGPUBufferManager(IRHIDevice& device, size_t maxDescriptorCount) override;
    virtual bool CreateGPUBuffer(IRHIDevice& device, const RHIBufferDesc& bufferDesc, size_t& outBufferIndex) override;
    
    ID3D12DescriptorHeap* GetDescriptorHeap() {return m_CBVDescriptorHeap.Get(); }
    
private:
    ComPtr<ID3D12DescriptorHeap> m_CBVDescriptorHeap;
    std::vector<std::shared_ptr<DX12GPUBuffer>> m_buffers;
};