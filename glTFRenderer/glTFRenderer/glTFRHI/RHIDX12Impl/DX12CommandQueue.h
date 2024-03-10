#pragma once
#include <d3d12.h>

#include "glTFRHI/RHIInterface/IRHICommandQueue.h"

class IRHIFence;

class DX12CommandQueue : public IRHICommandQueue
{
public:
    DX12CommandQueue();
    virtual ~DX12CommandQueue() override;
    
    virtual bool InitCommandQueue(IRHIDevice& device) override;
    
    ID3D12CommandQueue* GetCommandQueue() {return m_commandQueue; }
    const ID3D12CommandQueue* GetCommandQueue() const {return m_commandQueue; }
    
private:
    ID3D12CommandQueue* m_commandQueue;
};
