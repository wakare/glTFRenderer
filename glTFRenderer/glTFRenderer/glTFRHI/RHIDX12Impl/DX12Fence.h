#pragma once
#include "DX12Common.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"

class IRHICommandQueue;

class DX12Fence : public IRHIFence
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Fence)
    
    virtual bool InitFence(IRHIDevice& device) override;
    virtual bool HostWaitUtilSignaled() override;
    virtual bool ResetFence() override;
    
    bool SignalWhenCommandQueueFinish(IRHICommandQueue& commandQueue);
    
    virtual bool Release(glTFRenderResourceManager&) override;
    
private:
    ComPtr<ID3D12Fence> m_fence {nullptr};
    UINT64 m_fenceCompleteValue {0};
    HANDLE m_fenceEvent {nullptr};
};
