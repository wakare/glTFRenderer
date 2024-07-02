#include "glTFRenderMaterialManager.h"
#include "glTFMaterial/glTFMaterialOpaque.h"
#include "glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "RendererCommon.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "SceneFileLoader/glTFImageLoader.h"

glTFMaterialTextureRenderResource::glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& source_texture)
        : m_source_texture(source_texture)
        , m_texture(nullptr)
{
        
}

bool glTFMaterialTextureRenderResource::Init(glTFRenderResourceManager& resource_manager)
{
    auto& command_list = resource_manager.GetCommandListForRecord();

    const std::wstring convertPath = to_wide_string(m_source_texture.GetTexturePath());
    ImageLoadResult result;
    RETURN_IF_FALSE(glTFImageLoader::Instance().LoadImageByFilename(convertPath.c_str(), result))
    RHITextureDesc texture_desc{};
    texture_desc.Init(result);

    glTFRenderResourceManager::GetMemoryManager().AllocateTextureMemoryAndUpload(glTFRenderResourceManager::GetDevice(), command_list, texture_desc, m_texture);
    resource_manager.CloseCommandListAndExecute(false);
    
    return true;
}

const IRHITextureAllocation& glTFMaterialTextureRenderResource::GetTextureAllocation() const
{
    return *m_texture;
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
                    const auto& base_color_factor = dynamic_cast<const glTFMaterialParameterFactor<glm::vec4>&>(base_color_parameter);
                    m_factors[glTFMaterialParameterUsage::BASECOLOR] = std::make_unique<glTFMaterialParameterFactor<glm::vec4>>(base_color_factor);
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
                    const auto& normal_factor = dynamic_cast<const glTFMaterialParameterFactor<glm::vec4>&>(normal_parameter);
                    m_factors[glTFMaterialParameterUsage::NORMAL] = std::make_unique<glTFMaterialParameterFactor<glm::vec4>>(normal_factor);
                }
            }

            if (opaque_material.HasValidParameter(glTFMaterialParameterUsage::METALLIC_ROUGHNESS))
            {
                const auto& metallic_roughness_parameter = opaque_material.GetMaterialParameter(glTFMaterialParameterUsage::METALLIC_ROUGHNESS);
                if (metallic_roughness_parameter.GetParameterType() == Texture)
                {
                    const auto& metallic_roughness_texture = dynamic_cast<const glTFMaterialParameterTexture&>(metallic_roughness_parameter);
                    m_textures[glTFMaterialParameterUsage::METALLIC_ROUGHNESS] = std::make_unique<glTFMaterialTextureRenderResource>(metallic_roughness_texture);
                }
                else
                {
                    const auto& metallic_roughness_factor = dynamic_cast<const glTFMaterialParameterFactor<glm::vec4>&>(metallic_roughness_parameter);
                    m_factors[glTFMaterialParameterUsage::METALLIC_ROUGHNESS] = std::make_unique<glTFMaterialParameterFactor<glm::vec4>>(metallic_roughness_factor);
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

std::shared_ptr<IRHIDescriptorAllocation> glTFMaterialRenderResource::CreateOrGetAllTextureFirstGPUHandle(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap)
{
    std::shared_ptr<IRHIDescriptorAllocation> first_allocation;
    for (const auto& texture : m_textures)
    {
        std::shared_ptr<IRHIDescriptorAllocation> result;
        const auto& texture_resource = *texture.second->GetTextureAllocation().m_texture;
        descriptor_heap.CreateResourceDescriptorInHeap(resource_manager.GetDevice(), texture_resource,
            {
                .format = texture_resource.GetTextureDesc().GetDataFormat(),
                .dimension = RHIResourceDimension::TEXTURE2D,
                .view_type = RHIViewType::RVT_SRV,
            },
            result);
    }
    
    return first_allocation;
}

const std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialTextureRenderResource>>&
glTFMaterialRenderResource::GetTextures() const
{
    return m_textures;
}

const std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialParameterFactor<glm::vec4>>>&
glTFMaterialRenderResource::GetFactors() const
{
    return m_factors;
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
    GLTF_CHECK(false);   
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

        auto find_metallic_roughness_texture = material_textures.find(glTFMaterialParameterUsage::METALLIC_ROUGHNESS);
        if (find_metallic_roughness_texture != material_textures.end())
        {
            new_material_info.metallic_roughness_tex_index = gather_material_textures.size();
            gather_material_textures.push_back(find_metallic_roughness_texture->second.get());        
        }

        const auto& factors = material_resource.GetFactors();
        auto it_color = factors.find(glTFMaterialParameterUsage::BASECOLOR);
        new_material_info.albedo = it_color == factors.end() ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : it_color->second->GetFactor();

        auto it_normal = factors.find(glTFMaterialParameterUsage::NORMAL);
        new_material_info.normal = it_normal == factors.end() ? glm::vec4(0.0f, 0.0f, 1.0f, 0.0f) : it_normal->second->GetFactor();

        auto it_metallicAndRoughness = factors.find(glTFMaterialParameterUsage::METALLIC_ROUGHNESS);
        new_material_info.metallicAndRoughness = it_metallicAndRoughness == factors.end() ? glm::vec4(0.0f, 0.0f, 1.0f, 0.0f) : it_metallicAndRoughness->second->GetFactor();
    }
    
    return true;
}
