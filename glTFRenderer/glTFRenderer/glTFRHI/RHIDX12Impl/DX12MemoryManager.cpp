#include "DX12MemoryManager.h"
#include "d3dx12.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool DX12MemoryManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    std::shared_ptr<IRHIBuffer> dx12_buffer = RHIResourceFactory::CreateRHIResource<IRHIBuffer>();
    if (!dynamic_cast<DX12Buffer&>(*dx12_buffer).InitGPUBuffer(device, buffer_desc))
    {
        assert(false);
        return false;
    }

    out_buffer_allocation = std::make_shared<DX12BufferAllocation>();
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

bool DX12MemoryManager::AllocateTextureMemory(IRHIDevice& device, glTFRenderResourceManager& resource_manager,
                                              const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation)
{
    std::shared_ptr<IRHITexture> dx12_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    dynamic_cast<DX12Texture&>(*dx12_texture).InitTexture(device, resource_manager, texture_desc);

    out_texture_allocation = std::make_shared<IRHITextureAllocation>();
    out_texture_allocation->m_texture = dx12_texture;
    
    m_texture_allocations.push_back(out_texture_allocation);
    
    return true;
}

bool DX12MemoryManager::AllocateTextureMemoryAndUpload(IRHIDevice& device, glTFRenderResourceManager& resource_manager,
                                                       IRHICommandList& command_list, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation)
{
    std::shared_ptr<IRHITexture> dx12_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    dynamic_cast<DX12Texture&>(*dx12_texture).InitTextureAndUpload(device, resource_manager, command_list, texture_desc);

    out_texture_allocation = std::make_shared<IRHITextureAllocation>();
    out_texture_allocation->m_texture = dx12_texture;

    m_texture_allocations.push_back(out_texture_allocation);
    
    return true;
}

bool DX12MemoryManager::ReleaseMemoryAllocation(glTFRenderResourceManager& resource_manager,
    IRHIMemoryAllocation& memory_allocation)
{
    auto* raw_pointer = &memory_allocation;
    
    switch (memory_allocation.GetAllocationType()) {
    case IRHIMemoryAllocation::BUFFER:
        {
            bool removed = false;
            for (auto iter = m_buffer_allocations.begin(); iter != m_buffer_allocations.end(); ++iter)
            {
                if (iter->get() == raw_pointer)
                {
                    m_buffer_allocations.erase(iter);
                    removed = true;
                    break;
                }
            }

            auto buffer_resource = dynamic_cast<DX12Buffer&>(*dynamic_cast<DX12BufferAllocation&>(*raw_pointer).m_buffer).GetRawBuffer();
            buffer_resource->Release();
            
            GLTF_CHECK(removed);
        }
        
        break;
    case IRHIMemoryAllocation::TEXTURE:
        {
            bool removed = false;
            for (auto iter = m_texture_allocations.begin(); iter != m_texture_allocations.end(); ++iter)
            {
                if (iter->get() == raw_pointer)
                {
                    m_texture_allocations.erase(iter);
                    removed = true;
                    break;
                }
            }
            auto texture_resource = dynamic_cast<DX12Texture&>(*dynamic_cast<DX12TextureAllocation&>(*raw_pointer).m_texture).GetRawResource();
            texture_resource->Release();
            
            GLTF_CHECK(removed);
        }
        break;
    }

    return true;
}

bool DX12MemoryManager::ReleaseAllResource(glTFRenderResourceManager& resource_manager)
{
    // Clean all resources
    for (auto& texture  :m_texture_allocations)
    {
        auto texture_resource = dynamic_cast<DX12Texture&>(*dynamic_cast<DX12TextureAllocation&>(*texture).m_texture).GetRawResource();
        texture_resource->Release();
    }

    for (auto& buffer  :m_buffer_allocations)
    {
        auto texture_resource = dynamic_cast<DX12Buffer&>(*dynamic_cast<DX12BufferAllocation&>(*buffer).m_buffer).GetRawBuffer();
        texture_resource->Release();
    }
    
    m_texture_allocations.clear();
    m_buffer_allocations.clear();
    
    return true;
}
