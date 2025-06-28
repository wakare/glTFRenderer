#include "DX12Fence.h"

#include <cassert>

#include "DX12CommandQueue.h"
#include "DX12Device.h"
#include "DX12Utils.h"

bool DX12Fence::InitFence(IRHIDevice& device)
{
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(m_fenceEvent);
    
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    THROW_IF_FAILED( dxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))
    
    SetCanWait(true);
    need_release = true;
    
    return true;
}

bool DX12Fence::SignalWhenCommandQueueFinish(IRHICommandQueue& commandQueue)
{
    auto* dxCommandQueue = dynamic_cast<DX12CommandQueue&>(commandQueue).GetCommandQueue();
    THROW_IF_FAILED(dxCommandQueue->Signal(m_fence.Get(), ++m_fenceCompleteValue))

    return true;
}

bool DX12Fence::Release(IRHIMemoryManager& memory_manager)
{
    SAFE_RELEASE(m_fence)
    
    return true;
}

bool DX12Fence::HostWaitUtilSignaled()
{
    if (m_fence->GetCompletedValue() != m_fenceCompleteValue)
    {
        m_fence->SetEventOnCompletion(m_fenceCompleteValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    
    return true;
}

bool DX12Fence::ResetFence()
{
    return true;
}
