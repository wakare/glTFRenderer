#include "RHIUtils.h"

#include "RHIResourceFactoryImpl.hpp"

std::shared_ptr<RHIUtils> RHIUtils::g_instance = nullptr;

bool RHIUtils::UploadTextureData(IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIDevice& device,
    IRHITexture& dst, const RHITextureMipUploadInfo& upload_info)
{
    auto mip_texture_width = dst.GetTextureDesc().GetTextureWidth(upload_info.mip_level);
    auto mip_texture_height = dst.GetTextureDesc().GetTextureHeight(upload_info.mip_level);
    
    size_t mip_copy_row_pitch = dst.GetCopyReq().row_pitch[upload_info.mip_level];
    size_t mip_copy_total_size = mip_copy_row_pitch * mip_texture_height;
    
    const RHIBufferDesc texture_upload_buffer_desc =
        {
        L"TextureBuffer_Upload",
        mip_copy_total_size,
        1,
        1,
        RHIBufferType::Upload,
        dst.GetTextureFormat(),
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COPY_SOURCE,
        RUF_TRANSFER_SRC
    };

    std::shared_ptr<IRHIBufferAllocation> m_texture_upload_buffer;
    memory_manager.AllocateTempUploadBufferMemory(device, texture_upload_buffer_desc, m_texture_upload_buffer);

    size_t byte_per_pixel = GetBytePerPixelByFormat(dst.GetTextureFormat()); 
    std::vector<unsigned char> texture_data; texture_data.resize(mip_copy_total_size);
    unsigned char* texture_data_ptr = texture_data.data();
    for (unsigned h = 0; h < mip_texture_height; h++)
    {
        unsigned char* texture_data_offset_ptr = texture_data_ptr + mip_copy_row_pitch * h;
        unsigned char* src_ptr = upload_info.data.get() + mip_texture_width * h * byte_per_pixel;
        memcpy(texture_data_offset_ptr, src_ptr, mip_texture_width * byte_per_pixel);
    }
    
    memory_manager.UploadBufferData(*m_texture_upload_buffer, texture_data.data(), 0, mip_copy_total_size);

    RHICopyTextureInfo m_texture_upload_copy_info
    {
        0, 0, 0, upload_info.mip_level,
        mip_texture_width, mip_texture_height, static_cast<unsigned>(mip_copy_row_pitch)
    };
    
    CopyTexture(command_list, dst, *m_texture_upload_buffer->m_buffer, m_texture_upload_copy_info);
    
    return true;
}

bool RHIUtils::Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    return swap_chain.Present(command_queue, command_list);
}

RHIUtils& RHIUtils::Instance()
{
    if (!g_instance)
    {
        g_instance = RHIResourceFactory::CreateRHIResource<RHIUtils>();
    }

    return *g_instance;
}

void RHIUtils::ResetInstance()
{
    g_instance = RHIResourceFactory::CreateRHIResource<RHIUtils>();
}
