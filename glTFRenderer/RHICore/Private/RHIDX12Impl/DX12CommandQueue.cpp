#include "DX12CommandQueue.h"

#include "DX12Device.h"
#include "DX12Utils.h"
#include "RHIResourceFactory.h"
#include "RHIInterface/IRHIFence.h"

bool DX12CommandQueue::InitCommandQueue(IRHIDevice& device)
{
    // -- Create the Command Queue -- //
    D3D12_COMMAND_QUEUE_DESC cqDesc = {}; // we will be using all the default values

    auto dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    THROW_IF_FAILED(dxDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&m_command_queue))) // create the command queue
    THROW_IF_FAILED(dxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_idle_fence)))
    m_idle_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    GLTF_CHECK(m_idle_fence_event);

    need_release = true;
    
    return true;
}

bool DX12CommandQueue::WaitIdle()
{
    if (!m_command_queue || !m_idle_fence)
    {
        return true;
    }

    const UINT64 fence_value = ++m_idle_fence_value;
    THROW_IF_FAILED(m_command_queue->Signal(m_idle_fence.Get(), fence_value))
    if (m_idle_fence->GetCompletedValue() < fence_value)
    {
        THROW_IF_FAILED(m_idle_fence->SetEventOnCompletion(fence_value, m_idle_fence_event))
        WaitForSingleObject(m_idle_fence_event, INFINITE);
    }

    return true;
}

bool DX12CommandQueue::Release(IRHIMemoryManager& memory_manager)
{
    if (m_idle_fence_event)
    {
        CloseHandle(m_idle_fence_event);
        m_idle_fence_event = nullptr;
    }
    SAFE_RELEASE(m_idle_fence)
    m_idle_fence_value = 0;
    SAFE_RELEASE(m_command_queue)
    return true;
}
