#pragma once
#include "DX12Common.h"
#include "../RHIInterface/IRHIFence.h"

class DX12Fence : public IRHIFence
{
public:
    DX12Fence();
    virtual ~DX12Fence() override;
    
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool CommandQueueWaitForFence(IRHICommandQueue& commandQueue) override;
    virtual bool SignalWhenCommandQueueFinish(IRHICommandQueue& commandQueue) override;
    virtual bool HostWaitUtilSignaled() override;

private:
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceCompleteValue;
    HANDLE m_fenceEvent;
};
