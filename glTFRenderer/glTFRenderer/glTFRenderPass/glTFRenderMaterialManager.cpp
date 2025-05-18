#include "glTFRenderMaterialManager.h"
#include "glTFMaterial/glTFMaterialOpaque.h"
#include "RHIUtils.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RendererCommon.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

glTFMaterialTextureRenderResource::glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& source_texture)
        : m_source_texture(source_texture)
        , m_texture(nullptr)
{
        
}

bool glTFMaterialTextureRenderResource::Init(glTFRenderResourceManager& resource_manager)
{
    auto& command_list = resource_manager.GetCommandListForRecord();

    const std::wstring convert_path = to_wide_string(m_source_texture.GetTexturePath());
    ImageLoadResult result;
    RETURN_IF_FALSE(glTFImageIOUtil::Instance().LoadImageByFilename(convert_path.c_str(), result))
    RHITextureDesc texture_desc{};
    texture_desc.InitWithLoadedData(result);

    if (resource_manager.GetRenderSystem<VirtualTextureSystem>())
    {
        m_vt = texture_desc.GetTextureWidth() >= VT_TEXTURE_SIZE && texture_desc.GetTextureHeight() >= VT_TEXTURE_SIZE;
    }
    
    if (m_vt)
    {
        m_virtual_texture = std::make_shared<VTLogicalTextureSVT>();
        VTLogicalTextureConfig config{};
        config.virtual_texture_id = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetNextValidVTIdAndInc();
        m_virtual_texture->InitLogicalTexture(texture_desc, config);
    }
    else
    {
        resource_manager.GetMemoryManager().AllocateTextureMemoryAndUpload(resource_manager.GetDevice(), resource_manager, command_list, texture_desc, m_texture);
        // wait for upload??
        resource_manager.CloseCurrentCommandListAndExecute({}, false);    
    }
    
    return true;
}

bool glTFMaterialTextureRenderResource::IsVT() const
{
    return m_vt;
}

std::shared_ptr<VTLogicalTextureBase> glTFMaterialTextureRenderResource::GetVTTexture() const
{
    return m_virtual_texture;
}

const IRHITextureAllocation& glTFMaterialTextureRenderResource::GetTextureAllocation() const
{
    GLTF_CHECK(!IsVT());
    return *m_texture;
}

glTFMaterialRenderResource::glTFMaterialRenderResource(const glTFMaterialBase& source_material)
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

const std::map<glTFMaterialParameterUsage, std::unique_ptr<VTLogicalTextureBase>>& glTFMaterialRenderResource::
    GetVirtualTextures() const
{
    return m_virtual_textures;
}

bool glTFRenderMaterialManager::AddMaterialRenderResource(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material)
{
	const auto material_ID = material.GetID();
    if (m_material_render_resources.find(material_ID) == m_material_render_resources.end())
    {
        m_material_render_resources[material_ID] = std::make_unique<glTFMaterialRenderResource>(material);
        RETURN_IF_FALSE(m_material_render_resources[material_ID]->Init(resource_manager))    
    }

    return true;
}

void glTFRenderMaterialManager::Tick(glTFRenderResourceManager& resource_manager)
{
    // VT registration
    for (const auto& material : m_material_render_resources)
    {
        for (const auto& texture : material.second->GetTextures())
        {
            if (auto vt = texture.second->GetVTTexture())
            {
                if (resource_manager.GetRenderSystem<VirtualTextureSystem>() && !resource_manager.GetRenderSystem<VirtualTextureSystem>()->HasTexture(vt))
                {
                    resource_manager.GetRenderSystem<VirtualTextureSystem>()->RegisterTexture(vt);    
                }
            }
        }
    }
}

const std::map<glTFUniqueID, std::unique_ptr<glTFMaterialRenderResource>>& glTFRenderMaterialManager::
    GetMaterialRenderResources() const
{
    return m_material_render_resources;
}

