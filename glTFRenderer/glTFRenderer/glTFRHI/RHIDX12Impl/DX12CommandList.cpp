#include "DX12CommandList.h"

#include "DX12Device.h"
#include "DX12Utils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIFence.h"

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
    
    m_fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    m_fence->InitFence(device);

    m_finished_semaphore = RHIResourceFactory::CreateRHIResource<IRHISemaphore>();
    m_finished_semaphore->InitSemaphore(device);
    
    return true;
}

bool DX12CommandList::WaitCommandList()
{
    m_fence->HostWaitUtilSignaled();
    m_fence->ResetFence();
    return true;
}

bool DX12CommandList::BeginRecordCommandList()
{
    return true;
}

bool DX12CommandList::EndRecordCommandList()
{
    return true;
}
