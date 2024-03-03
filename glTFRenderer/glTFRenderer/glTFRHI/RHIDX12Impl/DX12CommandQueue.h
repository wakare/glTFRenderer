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
    virtual bool WaitCommandQueue() override;
    
    ID3D12CommandQueue* GetCommandQueue() {return m_commandQueue; }
    const ID3D12CommandQueue* GetCommandQueue() const {return m_commandQueue; }
    IRHIFence& GetFence() const {return *m_fence;}
    
private:
    ID3D12CommandQueue* m_commandQueue;

    std::shared_ptr<IRHIFence> m_fence;
};
