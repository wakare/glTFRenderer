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

bool glTFMaterialTextureRenderResource::Init(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    m_texture_buffer = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    RETURN_IF_FALSE(m_texture_buffer->UploadTextureFromFile(resource_manager.GetDevice(), command_list, m_source_texture.GetTexturePath()))
    
    resource_manager.CloseCommandListAndExecute(true);
    
    return true;
}

RHICPUDescriptorHandle glTFMaterialTextureRenderResource::CreateOrGetTextureSRVHandle(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap)
{
    if (!m_texture_SRV_handle)
    {
        // Create SRV within heap
        GLTF_CHECK(m_texture_buffer);
        
        RETURN_IF_FALSE(descriptor_heap.CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), descriptor_heap.GetUsedDescriptorCount(),
        m_texture_buffer->GetGPUBuffer(), {m_texture_buffer->GetTextureDesc().GetDataFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_texture_SRV_handle));
        
        GLTF_CHECK(m_texture_SRV_handle);
    }
    
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
    for (const auto& texture : m_textures)
    {
        RETURN_IF_FALSE(texture.second->Init(resource_manager, descriptor_heap))
    }
    
    return true;
}

RHIGPUDescriptorHandle glTFMaterialRenderResource::CreateOrGetAllTextureFirstGPUHandle(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap)
{
    RHIGPUDescriptorHandle handle = 0;
    for (const auto& texture : m_textures)
    {
        const auto current_handle = texture.second->CreateOrGetTextureSRVHandle(resource_manager, descriptor_heap);
        if (handle == 0 )
        {
            handle = current_handle;
        }
    }
    
    return handle;
}

bool glTFRenderMaterialManager::InitMaterialRenderResource(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap, const glTFMaterialBase& material)
{
	const auto material_ID = material.GetID();
    if (m_material_render_resources.find(material_ID) == m_material_render_resources.end())
    {
        m_material_render_resources[material_ID] = std::make_unique<glTFMaterialRenderResource>(material);
        RETURN_IF_FALSE(m_material_render_resources[material_ID]->Init(resource_manager, descriptor_heap))    
    }

    return true;
}

bool glTFRenderMaterialManager::ApplyMaterialRenderResource(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap, glTFUniqueID material_ID, unsigned slot_index)
{
    const auto find_material_iter = m_material_render_resources.find(material_ID);
    if (find_material_iter == m_material_render_resources.end())
    {
        GLTF_CHECK(false);
    }

    // Apply base color and normal now 
    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resource_manager.GetCommandListForRecord(),
            slot_index, find_material_iter->second->CreateOrGetAllTextureFirstGPUHandle(resource_manager, descriptor_heap)))

    return true;
}
