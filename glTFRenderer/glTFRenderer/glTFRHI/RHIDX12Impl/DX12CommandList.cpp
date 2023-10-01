#include "DX12CommandList.h"

#include "DX12Device.h"
#include "DX12Utils.h"

DX12CommandList::DX12CommandList()
    : m_command_list(nullptr)
    , m_dxr_command_list(nullptr)
{
}

DX12CommandList::~DX12CommandList()
{
    SAFE_RELEASE(m_command_list)
    SAFE_RELEASE(m_dxr_command_list)
}

bool DX12CommandList::InitCommandList(IRHIDevice& device, IRHICommandAllocator& commandAllocator)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxCommandAllocator = dynamic_cast<DX12CommandAllocator&>(commandAllocator).GetCommandAllocator();
    THROW_IF_FAILED(dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, dxCommandAllocator, nullptr, IID_PPV_ARGS(&m_command_list)))

    // Query DXR command list
    THROW_IF_FAILED(m_command_list->QueryInterface(IID_PPV_ARGS(&m_dxr_command_list)))
    
    // command lists are created in the recording state. our main loop will set it up for recording again so close it now
    m_command_list->Close();
    
    return true;
}
