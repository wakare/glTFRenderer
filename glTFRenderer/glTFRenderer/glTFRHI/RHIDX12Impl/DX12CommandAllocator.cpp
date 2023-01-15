#include "DX12CommandAllocator.h"
#include "DX12Device.h"

DX12CommandAllocator::DX12CommandAllocator()
    : m_commandAllocator(nullptr)
{
}

bool DX12CommandAllocator::InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    THROW_IF_FAILED(dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)))
    
    return true;
}
