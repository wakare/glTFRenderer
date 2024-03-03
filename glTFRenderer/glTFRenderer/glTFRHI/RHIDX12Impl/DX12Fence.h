#pragma once
#include "DX12Common.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"

class DX12Fence : public IRHIFence
{
public:
    DX12Fence();
    virtual ~DX12Fence() override;
    
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool HostWaitUtilSignaled() override;
    
    bool SignalWhenCommandQueueFinish(IRHICommandQueue& commandQueue);
    
private:
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceCompleteValue;
    HANDLE m_fenceEvent;
};
