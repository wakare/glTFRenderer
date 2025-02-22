#include "DX12CommandQueue.h"

#include "DX12Device.h"
#include "DX12Utils.h"
#include "glTFRHI/RHIResourceFactory.h"
#include "glTFRHI/RHIInterface/IRHIFence.h"

bool DX12CommandQueue::InitCommandQueue(IRHIDevice& device)
{
    // -- Create the Command Queue -- //
    D3D12_COMMAND_QUEUE_DESC cqDesc = {}; // we will be using all the default values

    auto dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    THROW_IF_FAILED(dxDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&m_command_queue))) // create the command queue

    need_release = true;
    
    return true;
}

bool DX12CommandQueue::Release(glTFRenderResourceManager&)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    SAFE_RELEASE(m_command_queue)
    return true;
}
