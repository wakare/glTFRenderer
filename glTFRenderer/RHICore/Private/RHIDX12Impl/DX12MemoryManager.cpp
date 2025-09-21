#include "DX12MemoryManager.h"
#include "d3dx12.h"
#include "RHIResourceFactoryImpl.hpp"


bool DX12MemoryManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
                                             std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    std::shared_ptr<IRHIBuffer> dx12_buffer = RHIResourceFactory::CreateRHIResource<IRHIBuffer>();
    if (!dynamic_cast<DX12Buffer&>(*dx12_buffer).CreateBuffer(device, buffer_desc))
    {
        assert(false);
        return false;
    }

    out_buffer_allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferAllocation>();
    out_buffer_allocation->m_buffer = dx12_buffer;
    out_buffer_allocation->SetNeedRelease();
    
    m_buffer_allocations.push_back(out_buffer_allocation);
    
    return true;
}

bool DX12MemoryManager::UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size)
{
    bool upload = false;
    if (buffer_allocation.m_buffer->GetBufferDesc().type == RHIBufferType::Upload)
    {
        // CPU visible buffer
        upload = dynamic_cast<DX12Buffer&>(*buffer_allocation.m_buffer).UploadBufferFromCPU(data, offset, size);
    }
    else
    {
        GLTF_CHECK(false);
    }
    
    return upload;
}

bool DX12MemoryManager::DownloadBufferData(IRHIBufferAllocation& buffer_allocation, void* data, size_t size)
{
    return dynamic_cast<DX12Buffer&>(*buffer_allocation.m_buffer).DownloadBufferToCPU(data, size);
}

bool DX12MemoryManager::AllocateTextureMemory(IRHIDevice& device, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation)
{
    std::shared_ptr<IRHIBufferAllocation> texture_buffer;
    std::shared_ptr<IRHITexture> dx12_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    if (!dynamic_cast<DX12Texture&>(*dx12_texture).CreateTexture(device, texture_desc))
    {
        assert(false);
        return false;
    }

    out_texture_allocation = RHIResourceFactory::CreateRHIResource<IRHITextureAllocation>();
    out_texture_allocation->m_texture = dx12_texture;
    out_texture_allocation->SetNeedRelease();
    
    m_texture_allocations.push_back(out_texture_allocation);
    
    return true;
}

bool DX12MemoryManager::ReleaseMemoryAllocation(IRHIMemoryAllocation& memory_allocation)
{
    auto* raw_pointer = &memory_allocation;
    
    switch (memory_allocation.GetAllocationType()) {
    case IRHIMemoryAllocation::BUFFER:
        {
            for (auto iter = m_buffer_allocations.begin(); iter != m_buffer_allocations.end(); ++iter)
            {
                if (iter->get() == raw_pointer)
                {
                    m_buffer_allocations.erase(iter);
                    break;
                }
            }
        }
        
        break;
    case IRHIMemoryAllocation::TEXTURE:
        {
            for (auto iter = m_texture_allocations.begin(); iter != m_texture_allocations.end(); ++iter)
            {
                if (iter->get() == raw_pointer)
                {
                    m_texture_allocations.erase(iter);
                    break;
                }
            }
        }
        break;
    }

    return true;
}