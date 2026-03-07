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
    const UINT64 signal_value = PredictNextSignalValue();
    THROW_IF_FAILED(dxCommandQueue->Signal(m_fence.Get(), signal_value))
    NotifySignalSubmitted(signal_value);

    return true;
}

unsigned long long DX12Fence::PredictNextSignalValue() const
{
    return m_fenceCompleteValue + 1ull;
}

void DX12Fence::NotifySignalSubmitted(unsigned long long signal_value)
{
    m_fenceCompleteValue = static_cast<UINT64>(signal_value);
}

bool DX12Fence::IsSignalValueCompleted(unsigned long long signal_value) const
{
    if (!m_fence || signal_value == 0ull)
    {
        return true;
    }

    return m_fence->GetCompletedValue() >= signal_value;
}

bool DX12Fence::Release(IRHIMemoryManager& memory_manager)
{
    SAFE_RELEASE(m_fence)
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_fenceCompleteValue = 0;
    
    return true;
}

bool DX12Fence::HostWaitUtilSignaled()
{
    if (!m_fence)
    {
        return true;
    }
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
