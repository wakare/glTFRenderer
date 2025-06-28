#include "RHIInterface/IRHIMemoryManager.h"
#include "RHIResourceFactoryImpl.hpp"

bool IRHIMemoryAllocation::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    return memory_manager.ReleaseMemoryAllocation( *this);
}

bool RHITempBufferPool::TryGetBuffer(const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer)
{
    for (auto& temp_upload_buffer : m_temp_upload_buffer_allocations)
    {
        if (temp_upload_buffer.desc == buffer_desc && temp_upload_buffer.remain_frame_to_reuse <= 0)
        {
            out_buffer = temp_upload_buffer.allocation;
            temp_upload_buffer.remain_frame_to_reuse = TempBufferInfo::TEMP_BUFFER_FRAME_LIFE_TIME;
            return true;
        }
    }

    return false;
}

void RHITempBufferPool::AddBufferToPool(const TempBufferInfo& buffer_info)
{
    m_temp_upload_buffer_allocations.push_back(buffer_info);
}

void RHITempBufferPool::TickFrame()
{
    for (auto& temp_upload_buffer : m_temp_upload_buffer_allocations)
    {
        temp_upload_buffer.remain_frame_to_reuse--;
    }
}

void RHITempBufferPool::Clear()
{
    m_temp_upload_buffer_allocations.clear();
}

bool IRHIMemoryManager::InitMemoryManager(IRHIDevice& device,
                                          const IRHIFactory& factory, const DescriptorAllocationInfo& descriptor_allocation_info)
{
    m_descriptor_manager = RHIResourceFactory::CreateRHIResource<IRHIDescriptorManager>();
    m_descriptor_manager->Init(device, descriptor_allocation_info);
    
    return true;
}

bool IRHIMemoryManager::AllocateTextureMemoryAndUpload(IRHIDevice& device,
                                                       IRHICommandList& command_list, IRHIMemoryManager& memory_manager,
                                                       const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation)
{
    AllocateTextureMemory(device, texture_desc, out_texture_allocation);

    GLTF_CHECK(texture_desc.HasTextureData());
    RHITextureMipUploadInfo upload_info
    {
        texture_desc.GetTextureData(),
        texture_desc.GetTextureDataSize(),
        0, 0, texture_desc.GetTextureWidth(), texture_desc.GetTextureHeight(), 0
    };
    
    RHIUtilInstanceManager::Instance().UploadTextureData(command_list, memory_manager, device, *out_texture_allocation->m_texture, upload_info);
    return true;
}

bool IRHIMemoryManager::ReleaseAllResource()
{
    m_texture_allocations.clear();
    m_buffer_allocations.clear();
    m_temp_buffer_pool.Clear();
    return true;
}

IRHIDescriptorManager& IRHIMemoryManager::GetDescriptorManager() const
{
    return *m_descriptor_manager;
}

void IRHIMemoryManager::TickFrame()
{
    m_temp_buffer_pool.TickFrame();
}

bool IRHIMemoryManager::AllocateTempUploadBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
                                                       std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    if (m_temp_buffer_pool.TryGetBuffer(buffer_desc, out_buffer_allocation))
    {
        return true;
    }

    RETURN_IF_FALSE(AllocateBufferMemory(device, buffer_desc, out_buffer_allocation))
    
    TempBufferInfo temp_buffer_info;
    temp_buffer_info.desc = buffer_desc;
    temp_buffer_info.remain_frame_to_reuse = TempBufferInfo::TEMP_BUFFER_FRAME_LIFE_TIME;
    temp_buffer_info.allocation = out_buffer_allocation;

    m_temp_buffer_pool.AddBufferToPool(temp_buffer_info);

    return true;
}
