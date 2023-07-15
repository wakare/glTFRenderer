#include "glTFRenderMaterialManager.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"
#include "../glTFRenderPass/glTFRenderPassManager.h"

glTFMaterialTextureRenderResource::glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& material)
        : m_source_texture(material)
        , m_texture_buffer(nullptr)
        , m_texture_SRV_handle(0)
{
        
}

bool glTFMaterialTextureRenderResource::Init(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap)
{
    glTFImageLoader imageLoader;
    imageLoader.InitImageLoader();

    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resource_manager.GetCommandList(), resource_manager.GetCurrentFrameCommandAllocator()))
    
    m_texture_buffer = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    RETURN_IF_FALSE(m_texture_buffer->UploadTextureFromFile(resource_manager.GetDevice(), resource_manager.GetCommandList(),
        imageLoader, m_source_texture.GetTexturePath()))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), descriptor_heap, 0,
        m_texture_buffer->GetGPUBuffer(), {m_texture_buffer->GetTextureDesc().GetDataFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_texture_SRV_handle))

    RHIUtils::Instance().CloseCommandList(resource_manager.GetCommandList()); 
    RHIUtils::Instance().ExecuteCommandList(resource_manager.GetCommandList(), resource_manager.GetCommandQueue());
    
    return true;
}

RHICPUDescriptorHandle glTFMaterialTextureRenderResource::GetTextureSRVHandle() const
{
    return m_texture_SRV_handle;
}

glTFMaterialRenderResource::glTFMaterialRenderResource(const glTFMaterialBase& source_texture)
    : m_source_texture(source_texture)
{
    
}
