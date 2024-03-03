#include "DX12CommandQueue.h"

#include "DX12Device.h"
#include "DX12Utils.h"
#include "glTFRHI/RHIResourceFactory.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"

DX12CommandQueue::DX12CommandQueue()
    : m_commandQueue(nullptr)
{
}

DX12CommandQueue::~DX12CommandQueue()
{
    SAFE_RELEASE(m_commandQueue);
}

bool DX12CommandQueue::InitCommandQueue(IRHIDevice& device)
{
    // -- Create the Command Queue -- //
    D3D12_COMMAND_QUEUE_DESC cqDesc = {}; // we will be using all the default values

    auto dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    THROW_IF_FAILED(dxDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&m_commandQueue))) // create the command queue

    m_fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    m_fence->InitFence(device);
    
    return true;
}

bool DX12CommandQueue::WaitCommandQueue()
{
    m_fence->HostWaitUtilSignaled();
    return true;
}
