#include "glTFRenderMaterialManager.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"
#include "../glTFRenderPass/glTFRenderPassManager.h"

glTFMaterialTextureRenderResource::glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& source_texture)
        : m_source_texture(source_texture)
        , m_texture_buffer(nullptr)
        , m_texture_SRV_handle(0)
{
        
}

bool glTFMaterialTextureRenderResource::Init(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap, unsigned descriptor_offset)
{
    glTFImageLoader imageLoader;
    imageLoader.InitImageLoader();

    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resource_manager.GetCommandList(), resource_manager.GetCurrentFrameCommandAllocator()))
    
    m_texture_buffer = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    RETURN_IF_FALSE(m_texture_buffer->UploadTextureFromFile(resource_manager.GetDevice(), resource_manager.GetCommandList(),
        imageLoader, m_source_texture.GetTexturePath()))

    RETURN_IF_FALSE(descriptor_heap.CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), descriptor_heap.GetUsedDescriptorCount(),
        m_texture_buffer->GetGPUBuffer(), {m_texture_buffer->GetTextureDesc().GetDataFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_texture_SRV_handle));

    RHIUtils::Instance().CloseCommandList(resource_manager.GetCommandList()); 
    RHIUtils::Instance().ExecuteCommandList(resource_manager.GetCommandList(), resource_manager.GetCommandQueue());
    
    return true;
}

RHICPUDescriptorHandle glTFMaterialTextureRenderResource::GetTextureSRVHandle() const
{
    return m_texture_SRV_handle;
}

glTFMaterialRenderResource::glTFMaterialRenderResource(const glTFMaterialBase& source_material)
    : m_source_material(source_material)
{
    switch (source_material.GetMaterialType())
    {
    case MaterialType::Opaque:
        {
            const glTFMaterialOpaque& opaque_material = dynamic_cast<const glTFMaterialOpaque&>(source_material);
            if (opaque_material.HasValidParameter(glTFMaterialParameterUsage::BASECOLOR))
            {
                const auto& base_color_parameter = opaque_material.GetMaterialParameter(glTFMaterialParameterUsage::BASECOLOR);
                if (base_color_parameter.GetParameterType() == Texture)
                {
                    const auto& base_color_texture = dynamic_cast<const glTFMaterialParameterTexture&>(base_color_parameter);
                    m_textures[glTFMaterialParameterUsage::BASECOLOR] = std::make_unique<glTFMaterialTextureRenderResource>(base_color_texture);
                }
                else if (base_color_parameter.GetParameterType() == Factor)
                {
                    // TODO:
                }
            }

            if (opaque_material.HasValidParameter(glTFMaterialParameterUsage::NORMAL))
            {
                const auto& normal_parameter = opaque_material.GetMaterialParameter(glTFMaterialParameterUsage::NORMAL);
                if (normal_parameter.GetParameterType() == Texture)
                {
                    const auto& normal_texture = dynamic_cast<const glTFMaterialParameterTexture&>(normal_parameter);
                    m_textures[glTFMaterialParameterUsage::NORMAL] = std::make_unique<glTFMaterialTextureRenderResource>(normal_texture);
                }
                else
                {
                    // TODO: 
                }
            }
        }
        break;
        
    case MaterialType::Translucent: break;
    case MaterialType::Unknown: break;
    default: ;
    }
}

bool glTFMaterialRenderResource::Init(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap)
{
    unsigned texture_index = 0;
    for (const auto& texture : m_textures)
    {
        RETURN_IF_FALSE(texture.second->Init(resource_manager, descriptor_heap, texture_index++))
    }
    
    return true;
}

RHIGPUDescriptorHandle glTFMaterialRenderResource::GetTextureGPUHandle() const
{
    GLTF_CHECK(!m_textures.empty() && m_textures.find(glTFMaterialParameterUsage::BASECOLOR) != m_textures.end());
    return m_textures.find(glTFMaterialParameterUsage::BASECOLOR)->second->GetTextureSRVHandle();
}

bool glTFRenderMaterialManager::InitMaterialRenderResource(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap, const glTFMaterialBase& material)
{
	const auto material_ID = material.GetID();
	m_material_render_resources[material_ID] = std::make_unique<glTFMaterialRenderResource>(material);
    RETURN_IF_FALSE(m_material_render_resources[material_ID]->Init(resource_manager, descriptor_heap))

    return true;
}

const glTFMaterialRenderResource& glTFRenderMaterialManager::GetMaterialResource(glTFUniqueID material_ID) const
{
    const auto find_material_iter = m_material_render_resources.find(material_ID);
    if (find_material_iter == m_material_render_resources.end())
    {
        GLTF_CHECK(false);
    }
    
    return *find_material_iter->second;
}

bool glTFRenderMaterialManager::ApplyMaterialRenderResource(glTFRenderResourceManager& resource_manager, glTFUniqueID material_ID, unsigned slot_index)
{
    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resource_manager.GetCommandList(),
            slot_index, GetMaterialResource(material_ID).GetTextureGPUHandle()))

    return true;
}
