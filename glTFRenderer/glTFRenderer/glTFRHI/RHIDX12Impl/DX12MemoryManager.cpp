#include "DX12MemoryManager.h"

#include <cassert>

#include "d3dx12.h"
#include "DX12Device.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

DX12MemoryManager::~DX12MemoryManager()
{
    SAFE_RELEASE(m_CBV_SRV_UAV_Heap)
}

bool DX12MemoryManager::InitMemoryManager(IRHIDevice& device, std::shared_ptr<IRHIMemoryAllocator> memory_allocator, unsigned max_descriptor_count)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = max_descriptor_count;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    THROW_IF_FAILED(dxDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_CBV_SRV_UAV_Heap)))

    m_allocator = memory_allocator;
    
    return true;
}

bool DX12MemoryManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    std::shared_ptr<IRHIBuffer> dx12_buffer = RHIResourceFactory::CreateRHIResource<IRHIBuffer>();
    if (!dx12_buffer->InitGPUBuffer(device, buffer_desc))
    {
        assert(false);
        return false;
    }

    m_buffers.push_back(dx12_buffer);
    out_buffer_allocation = std::make_shared<DX12BufferAllocation>();
    out_buffer_allocation->m_buffer = dx12_buffer;
    
    return true;
}

bool DX12MemoryManager::UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size)
{
    return buffer_allocation.m_buffer->UploadBufferFromCPU(data, offset, size);
}
