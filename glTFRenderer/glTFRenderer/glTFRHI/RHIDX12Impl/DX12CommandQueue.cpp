#include "DX12CommandQueue.h"

#include "DX12Device.h"

DX12CommandQueue::DX12CommandQueue()
    : m_commandQueue(nullptr)
{
}

bool DX12CommandQueue::InitCommandQueue(IRHIDevice& device)
{
    // -- Create the Command Queue -- //
    D3D12_COMMAND_QUEUE_DESC cqDesc = {}; // we will be using all the default values

    auto dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    HRESULT hr = dxDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&m_commandQueue)); // create the command queue
    if (FAILED(hr))
    {
        return false;
    }
    
    return true;
}
