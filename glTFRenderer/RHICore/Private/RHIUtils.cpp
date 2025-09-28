#include "RHIUtils.h"

#include <mutex>

#include "RHIResourceFactoryImpl.hpp"

bool RHIUtils::UploadTextureData(IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIDevice& device,
    IRHITexture& dst, const RHITextureMipUploadInfo& upload_info)
{
    size_t mip_copy_row_pitch = dst.GetCopyReq().row_pitch[upload_info.mip_level];
    GLTF_CHECK(upload_info.width && upload_info.height);
    size_t mip_copy_total_size = mip_copy_row_pitch * upload_info.height;
    
    const RHIBufferDesc texture_upload_buffer_desc =
        {
        L"TextureBuffer_Upload",
        mip_copy_total_size,
        1,
        1,
        RHIBufferType::Upload,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COPY_SOURCE,
        RUF_TRANSFER_SRC
    };

    std::shared_ptr<IRHIBufferAllocation> m_texture_upload_buffer;
    memory_manager.AllocateTempUploadBufferMemory(device, texture_upload_buffer_desc, m_texture_upload_buffer);

    size_t byte_per_pixel = GetBytePerPixelByFormat(dst.GetTextureFormat()); 
    std::vector<unsigned char> texture_data; texture_data.resize(mip_copy_total_size);
    unsigned char* texture_data_ptr = texture_data.data();
    for (unsigned h = 0; h < upload_info.height; h++)
    {
        unsigned char* texture_data_offset_ptr = texture_data_ptr + mip_copy_row_pitch * h;
        unsigned char* src_ptr = upload_info.data.get() + upload_info.width * h * byte_per_pixel;
        memcpy(texture_data_offset_ptr, src_ptr, mip_copy_row_pitch);
    }
    
    memory_manager.UploadBufferData(device, command_list, *m_texture_upload_buffer, texture_data.data(), 0, mip_copy_total_size);

    RHICopyTextureInfo m_texture_upload_copy_info
    {
        upload_info.dst_x, upload_info.dst_y, 0, upload_info.mip_level,
        upload_info.width, upload_info.height, static_cast<unsigned>(mip_copy_row_pitch)
    };
    
    CopyTexture(command_list, dst, *m_texture_upload_buffer->m_buffer, m_texture_upload_copy_info);
    
    return true;
}

bool RHIUtils::Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    return swap_chain.Present(command_queue, command_list);
}


namespace {
    std::shared_ptr<RHIUtils> g_instance = nullptr;
    std::mutex g_mutex;
}

RHIUtils& RHIUtilInstanceManager::Instance()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_instance)
    {
        g_instance = RHIResourceFactory::CreateRHIResource<RHIUtils>();
    }
    
    return *g_instance;
}

void RHIUtilInstanceManager::ResetInstance()
{
    g_instance = RHIResourceFactory::CreateRHIResource<RHIUtils>();
}
