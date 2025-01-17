#include "DX12MemoryManager.h"
#include "d3dx12.h"
#include "DX12Device.h"
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

    m_buffers.push_back(dx12_buffer);
    out_buffer_allocation = std::make_shared<DX12BufferAllocation>();
    out_buffer_allocation->m_buffer = dx12_buffer;
    
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
                                              const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    std::shared_ptr<IRHITexture> dx12_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    dynamic_cast<DX12Texture&>(*dx12_texture).InitTexture(device, resource_manager, texture_desc);

    m_textures.push_back(dx12_texture);
    out_buffer_allocation = std::make_shared<IRHITextureAllocation>();
    out_buffer_allocation->m_texture = dx12_texture;
    return true;
}

bool DX12MemoryManager::AllocateTextureMemoryAndUpload(IRHIDevice& device, glTFRenderResourceManager& resource_manager,
                                                       IRHICommandList& command_list, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    std::shared_ptr<IRHITexture> dx12_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    dynamic_cast<DX12Texture&>(*dx12_texture).InitTextureAndUpload(device, resource_manager, command_list, texture_desc);

    out_buffer_allocation = std::make_shared<IRHITextureAllocation>();
    out_buffer_allocation->m_texture = dx12_texture;
    m_textures.push_back(dx12_texture);
    
    return true;
}

bool DX12MemoryManager::CleanAllocatedResource()
{
    // Clean all resources
    m_textures.clear();
    m_buffers.clear();
    m_allocator = nullptr;
    
    return true;
}
