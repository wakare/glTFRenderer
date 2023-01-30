#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIFence.h"

class DX12Fence : public IRHIFence
{
public:
    DX12Fence();
    virtual ~DX12Fence() override;
    
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool SignalWhenCommandQueueFinish(IRHICommandQueue& commandQueue) override;
    virtual bool WaitUtilSignal() override;

private:
    ID3D12Fence* m_fence;
    UINT64 m_fenceCompleteValue;
    HANDLE m_fenceEvent;
};
