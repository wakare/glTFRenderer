#include "glTFRenderMaterialManager.h"
#include "glTFMaterial/glTFMaterialOpaque.h"
#include "glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFUtils/glTFImageLoader.h"
#include "glTFRenderPass/glTFRenderPassManager.h"

glTFMaterialTextureRenderResource::glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& source_texture)
        : m_source_texture(source_texture)
        , m_texture_buffer(nullptr)
{
        
}

bool glTFMaterialTextureRenderResource::Init(glTFRenderResourceManager& resource_manager)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    m_texture_buffer = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    RETURN_IF_FALSE(m_texture_buffer->UploadTextureFromFile(resource_manager.GetDevice(), command_list, m_source_texture.GetTexturePath()))

    resource_manager.CloseCommandListAndExecute(false);
    
    return true;
}

RHICPUDescriptorHandle glTFMaterialTextureRenderResource::CreateTextureSRVHandleInHeap(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap) const
{
    RHICPUDescriptorHandle result = 0;
    
    GLTF_CHECK(m_texture_buffer);
        
    RETURN_IF_FALSE(descriptor_heap.CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), descriptor_heap.GetUsedDescriptorCount(),
    m_texture_buffer->GetGPUBuffer(), {m_texture_buffer->GetTextureDesc().GetDataFormat(), RHIResourceDimension::TEXTURE2D}, result))
        
    GLTF_CHECK(result);
    
    return result;
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

bool glTFMaterialRenderResource::Init(glTFRenderResourceManager& resource_manager)
{
    for (const auto& texture : m_textures)
    {
        RETURN_IF_FALSE(texture.second->Init(resource_manager))
    }
    
    return true;
}

RHIGPUDescriptorHandle glTFMaterialRenderResource::CreateOrGetAllTextureFirstGPUHandle(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap)
{
    RHIGPUDescriptorHandle handle = 0;
    for (const auto& texture : m_textures)
    {
        const auto current_handle = texture.second->CreateTextureSRVHandleInHeap(resource_manager, descriptor_heap);
        if (handle == 0 )
        {
            handle = current_handle;
        }
    }
    
    return handle;
}

const std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialTextureRenderResource>>&
glTFMaterialRenderResource::GetTextures() const
{
    return m_textures;
}

bool glTFRenderMaterialManager::InitMaterialRenderResource(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material)
{
	const auto material_ID = material.GetID();
    if (m_material_render_resources.find(material_ID) == m_material_render_resources.end())
    {
        m_material_render_resources[material_ID] = std::make_unique<glTFMaterialRenderResource>(material);
        RETURN_IF_FALSE(m_material_render_resources[material_ID]->Init(resource_manager))    
    }

    return true;
}

bool glTFRenderMaterialManager::ApplyMaterialRenderResource(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap, glTFUniqueID material_ID, unsigned slot_index, bool
                                                            isGraphicsPipeline)
{
    const auto find_material_iter = m_material_render_resources.find(material_ID);
    if (find_material_iter == m_material_render_resources.end())
    {
        GLTF_CHECK(false);
    }

    // Apply base color and normal now 
    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(resource_manager.GetCommandListForRecord(),
            slot_index, find_material_iter->second->CreateOrGetAllTextureFirstGPUHandle(resource_manager, descriptor_heap), isGraphicsPipeline))

    return true;
}

bool glTFRenderMaterialManager::GatherAllMaterialRenderResource(
    std::vector<MaterialInfo>& gather_material_infos, std::vector<glTFMaterialTextureRenderResource*>&
    gather_material_textures) const
{
    gather_material_infos.resize(m_material_render_resources.size());
    gather_material_textures.reserve(m_material_render_resources.size() * 2);
    
    for (const auto& material_info : m_material_render_resources)
    {
        const auto material_id = material_info.first;
        
        const auto& material_resource = *material_info.second;
        const auto& material_textures = material_resource.GetTextures();
        MaterialInfo& new_material_info = gather_material_infos[material_id];
        
        auto find_albedo_texture = material_textures.find(glTFMaterialParameterUsage::BASECOLOR);
        if (find_albedo_texture != material_textures.end())
        {
            new_material_info.albedo_tex_index = gather_material_textures.size();
            gather_material_textures.push_back(find_albedo_texture->second.get());        
        }

        auto find_normal_texture = material_textures.find(glTFMaterialParameterUsage::NORMAL);
        if (find_normal_texture != material_textures.end())
        {
            new_material_info.normal_tex_index = gather_material_textures.size();
            gather_material_textures.push_back(find_normal_texture->second.get());        
        }
    }
    
    return true;
}
