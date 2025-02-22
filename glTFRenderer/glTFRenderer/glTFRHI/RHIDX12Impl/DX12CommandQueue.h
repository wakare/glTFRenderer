#pragma once
#include "DX12Common.h"
#include "glTFRHI/RHIInterface/IRHICommandQueue.h"

class IRHIFence;

class DX12CommandQueue : public IRHICommandQueue
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12CommandQueue)
    
    virtual bool InitCommandQueue(IRHIDevice& device) override;
    
    ID3D12CommandQueue* GetCommandQueue() {return m_command_queue.Get(); }
    const ID3D12CommandQueue* GetCommandQueue() const {return m_command_queue.Get(); }

    virtual bool Release(glTFRenderResourceManager&) override;
    
private:
    ComPtr<ID3D12CommandQueue> m_command_queue {nullptr};
};
