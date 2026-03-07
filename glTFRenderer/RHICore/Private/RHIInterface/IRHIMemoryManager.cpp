#include "RHIInterface/IRHIMemoryManager.h"
#include "RHIResourceFactoryImpl.hpp"
#include <algorithm>

bool IRHIMemoryAllocation::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    return memory_manager.ReleaseMemoryAllocation( *this);
}

RHIBufferDesc IRHIMemoryManager::MakeTempUploadBufferDesc(const RHIBufferDesc& dst_buffer_desc, size_t upload_size)
{
    RHIBufferDesc upload_desc{};
    upload_desc.name = L"TempUploadBuffer";
    upload_desc.width = upload_size;
    upload_desc.height = 1;
    upload_desc.depth = 1;
    upload_desc.type = RHIBufferType::Upload;
    upload_desc.resource_type = RHIBufferResourceType::Buffer;
    upload_desc.state = RHIResourceStateType::STATE_COPY_SOURCE;
    upload_desc.usage = RUF_TRANSFER_SRC;
    upload_desc.alignment = 0;

    if (dst_buffer_desc.resource_type != RHIBufferResourceType::Buffer)
    {
        upload_desc.resource_type = dst_buffer_desc.resource_type;
        upload_desc.height = dst_buffer_desc.height;
        upload_desc.depth = dst_buffer_desc.depth;
    }

    return upload_desc;
}

RHITempBufferPool::TempBufferBucketKey RHITempBufferPool::BuildBucketKey(const RHIBufferDesc& buffer_desc)
{
    TempBufferBucketKey key{};
    key.height = buffer_desc.height;
    key.depth = buffer_desc.depth;
    key.alignment = buffer_desc.alignment;
    key.type = buffer_desc.type;
    key.resource_type = buffer_desc.resource_type;
    key.state = buffer_desc.state;
    key.usage = buffer_desc.usage;
    return key;
}

size_t RHITempBufferPool::TempBufferBucketKeyHasher::operator()(const TempBufferBucketKey& key) const noexcept
{
    const auto hash_combine = [](size_t seed, size_t value) -> size_t
    {
        return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u));
    };

    size_t seed = 0;
    seed = hash_combine(seed, key.height);
    seed = hash_combine(seed, key.depth);
    seed = hash_combine(seed, key.alignment);
    seed = hash_combine(seed, static_cast<size_t>(key.type));
    seed = hash_combine(seed, static_cast<size_t>(key.resource_type));
    seed = hash_combine(seed, static_cast<size_t>(key.state));
    seed = hash_combine(seed, static_cast<size_t>(key.usage));
    return seed;
}

bool RHITempBufferPool::TryGetBuffer(const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer)
{
    const auto bucket_key = BuildBucketKey(buffer_desc);
    const auto bucket_it = m_temp_upload_buffer_allocations.find(bucket_key);
    if (bucket_it == m_temp_upload_buffer_allocations.end())
    {
        return false;
    }

    auto& bucket = bucket_it->second;
    const auto begin_it = std::lower_bound(
        bucket.begin(),
        bucket.end(),
        buffer_desc.width,
        [](const TempBufferInfo& buffer_info, size_t requested_width)
        {
            return buffer_info.desc.width < requested_width;
        });
    for (auto it = begin_it; it != bucket.end(); ++it)
    {
        auto& temp_upload_buffer = *it;
        if (temp_upload_buffer.remain_frame_to_reuse <= 0 &&
            temp_upload_buffer.desc.width >= buffer_desc.width)
        {
            out_buffer = temp_upload_buffer.allocation;
            temp_upload_buffer.remain_frame_to_reuse = TempBufferInfo::TEMP_BUFFER_FRAME_LIFE_TIME;
            temp_upload_buffer.idle_frame_count = 0;
            return true;
        }
    }

    return false;
}

void RHITempBufferPool::AddBufferToPool(const TempBufferInfo& buffer_info)
{
    auto& bucket = m_temp_upload_buffer_allocations[BuildBucketKey(buffer_info.desc)];
    const auto insert_it = std::lower_bound(
        bucket.begin(),
        bucket.end(),
        buffer_info.desc.width,
        [](const TempBufferInfo& existing_buffer_info, size_t requested_width)
        {
            return existing_buffer_info.desc.width < requested_width;
        });
    bucket.insert(insert_it, buffer_info);
}

void RHITempBufferPool::TickFrame(std::vector<std::shared_ptr<IRHIBufferAllocation>>& out_expired_buffers)
{
    for (auto bucket_it = m_temp_upload_buffer_allocations.begin(); bucket_it != m_temp_upload_buffer_allocations.end();)
    {
        auto& bucket = bucket_it->second;
        for (auto buffer_it = bucket.begin(); buffer_it != bucket.end();)
        {
            auto& temp_upload_buffer = *buffer_it;
            temp_upload_buffer.remain_frame_to_reuse--;
            if (temp_upload_buffer.remain_frame_to_reuse <= 0)
            {
                temp_upload_buffer.remain_frame_to_reuse = 0;
                ++temp_upload_buffer.idle_frame_count;
                if (temp_upload_buffer.idle_frame_count > TempBufferInfo::TEMP_BUFFER_MAX_IDLE_FRAME_COUNT)
                {
                    out_expired_buffers.push_back(temp_upload_buffer.allocation);
                    buffer_it = bucket.erase(buffer_it);
                    continue;
                }
            }
            else
            {
                temp_upload_buffer.idle_frame_count = 0;
            }

            ++buffer_it;
        }

        if (bucket.empty())
        {
            bucket_it = m_temp_upload_buffer_allocations.erase(bucket_it);
        }
        else
        {
            ++bucket_it;
        }
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

bool IRHIMemoryManager::UploadBufferData(IRHIDevice& device, IRHICommandList& command_list, IRHIBufferAllocation& buffer_allocation,
                                         const void* data, size_t dst_offset, size_t size)
{
    bool result = false;
    if (buffer_allocation.m_buffer->GetBufferDesc().type == RHIBufferType::Default)
    {
        const RHIBufferDesc upload_desc = MakeTempUploadBufferDesc(buffer_allocation.m_buffer->GetBufferDesc(), size);
        std::shared_ptr<IRHIBufferAllocation> upload_buffer = nullptr;
        bool allocated = AllocateTempUploadBufferMemory(device, upload_desc, upload_buffer);
        GLTF_CHECK(allocated);

        result = UploadBufferDataInner(*upload_buffer, data, 0, size);
        GLTF_CHECK(result);
        
        result = RHIUtilInstanceManager::Instance().CopyBuffer(command_list, *buffer_allocation.m_buffer, dst_offset, *upload_buffer->m_buffer, 0, size);
    }
    else
    {
        result = UploadBufferDataInner(buffer_allocation, data, dst_offset, size);
    }

    GLTF_CHECK(result);
    return result;
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
    std::vector<std::shared_ptr<IRHIBufferAllocation>> expired_temp_buffers;
    m_temp_buffer_pool.TickFrame(expired_temp_buffers);
    for (auto& expired_temp_buffer : expired_temp_buffers)
    {
        if (expired_temp_buffer)
        {
            expired_temp_buffer->Release(*this);
        }
    }
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
    temp_buffer_info.idle_frame_count = 0;
    temp_buffer_info.allocation = out_buffer_allocation;

    m_temp_buffer_pool.AddBufferToPool(temp_buffer_info);

    return true;
}
