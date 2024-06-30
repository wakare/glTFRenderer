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

    m_CBV_SRV_UAV_Heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_CBV_SRV_UAV_Heap->InitDescriptorHeap(device,
        {
            .maxDescriptorCount = max_descriptor_count,
            .type = RHIDescriptorHeapType::CBV_SRV_UAV,
            .shaderVisible = true
        });

    m_RTV_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_RTV_heap->InitDescriptorHeap(device,
        {
            .maxDescriptorCount = max_descriptor_count,
            .type = RHIDescriptorHeapType::RTV,
            .shaderVisible = false
        });

    m_DSV_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_DSV_heap->InitDescriptorHeap(device,
        {
            .maxDescriptorCount = max_descriptor_count,
            .type = RHIDescriptorHeapType::DSV,
            .shaderVisible = false
        });

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

bool DX12MemoryManager::AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list,
                                                       const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    std::shared_ptr<IRHITexture> dx12_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    dx12_texture->InitTexture(device, command_list, texture_desc);

    out_buffer_allocation = std::make_shared<IRHITextureAllocation>();
    out_buffer_allocation->m_texture = dx12_texture;
    m_textures.push_back(dx12_texture);
    
    return true;
}