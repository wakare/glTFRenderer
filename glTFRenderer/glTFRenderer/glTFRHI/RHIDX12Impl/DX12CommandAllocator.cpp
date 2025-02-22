#include "DX12CommandAllocator.h"
#include "DX12Device.h"
#include "DX12Utils.h"

bool DX12CommandAllocator::InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    THROW_IF_FAILED(dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocator)))

    need_release = true;
    
    return true;
}

bool DX12CommandAllocator::Release(glTFRenderResourceManager&)
{
    SAFE_RELEASE(m_command_allocator);
    return true;
}
