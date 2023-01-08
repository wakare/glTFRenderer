#include "DX12CommandList.h"

#include "DX12Device.h"

DX12CommandList::DX12CommandList()
    : m_commandList(nullptr)
{
}

bool DX12CommandList::InitCommandList(IRHIDevice& device, IRHICommandAllocator& commandAllocator)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();
    HRESULT hr = dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, dxCommandAllocator, nullptr, IID_PPV_ARGS(&m_commandList));
    if (FAILED(hr))
    {
        return false;
    }

    // command lists are created in the recording state. our main loop will set it up for recording again so close it now
    m_commandList->Close();
    return true;
}
