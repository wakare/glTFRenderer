#include "RHIUtils.h"

#include "RHIResourceFactoryImpl.hpp"

std::shared_ptr<RHIUtils> RHIUtils::g_instance = nullptr;

bool RHIUtils::UploadTextureData(IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIDevice& device,
    IRHITexture& dst, const RHITextureUploadInfo& upload_info)
{
    const size_t bytes_per_pixel = GetBytePerPixelByFormat(dst.GetTextureFormat());
    const size_t image_bytes_per_row = bytes_per_pixel * dst.GetTextureDesc().GetTextureWidth(); 
    const UINT64 texture_upload_buffer_size = ((image_bytes_per_row + 255 ) & ~255) * (dst.GetTextureDesc().GetTextureHeight() - 1) + image_bytes_per_row;
    
    const RHIBufferDesc texture_upload_buffer_desc =
        {
        L"TextureBuffer_Upload",
        texture_upload_buffer_size,
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
    memory_manager.UploadBufferData(*m_texture_upload_buffer, upload_info.data.get(), 0, upload_info.data_size);
    CopyTexture(command_list, dst, *m_texture_upload_buffer->m_buffer, {});
    
    return true;
}

bool RHIUtils::UploadTextureMipData(IRHICommandList& command_list, IRHIMemoryManager& memory_manager,
    IRHIDevice& device, IRHITexture& dst, const RHITextureMipUploadInfo& upload_info)
{
    auto mip_texture_width = dst.GetTextureDesc().GetTextureWidth(upload_info.mip_level);
    auto mip_texture_height = dst.GetTextureDesc().GetTextureHeight(upload_info.mip_level);
    
    const size_t bytes_per_pixel = GetBytePerPixelByFormat(dst.GetTextureFormat());
    const size_t image_bytes_per_row = bytes_per_pixel * mip_texture_width; 
    const UINT64 texture_upload_buffer_size = ((image_bytes_per_row + 255 ) & ~255) * (mip_texture_height - 1) + image_bytes_per_row;
    
    const RHIBufferDesc texture_upload_buffer_desc =
        {
        L"TextureBuffer_Upload",
        texture_upload_buffer_size,
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
    memory_manager.UploadBufferData(*m_texture_upload_buffer, upload_info.data.get(), 0, upload_info.data_size);

    RHICopyTextureInfo m_texture_upload_copy_info
    {
        0, 0, 0, upload_info.mip_level,
        mip_texture_width, mip_texture_height
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
