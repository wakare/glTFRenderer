#include "RendererModuleMaterial.h"
#include "RendererSceneCommon.h"

RendererModuleMaterial::RendererModuleMaterial(RendererInterface::ResourceOperator& resource_operator)
    :m_resource_operator(resource_operator) 
{
    
}

bool RendererModuleMaterial::AddMaterial(const MaterialBase& material)
{
    if (m_material_shader_infos.contains(material.GetID()))
    {
        return true;
    }
    
    MaterialShaderInfo material_shader_info{};
    if (material.GetParameter(MaterialBase::MaterialParameterUsage::BASE_COLOR))
    {
        auto base_color_parameter = material.GetParameter(MaterialBase::MaterialParameterUsage::BASE_COLOR);
        if (base_color_parameter->GetType() == MaterialParameter::MaterialParameterType::TEXTURE)
        {
            material_shader_info.albedo_tex_index = m_material_texture_uris.size();
            m_material_texture_uris.push_back(base_color_parameter->GetTexture());
        }
        else
        {
            material_shader_info.albedo = base_color_parameter->GetFactor();
        }
    }

    if (material.HasParameter(MaterialBase::MaterialParameterUsage::NORMAL) && material.GetParameter(MaterialBase::MaterialParameterUsage::NORMAL))
    {
        auto normal_parameter = material.GetParameter(MaterialBase::MaterialParameterUsage::NORMAL);
        if (normal_parameter->GetType() == MaterialParameter::MaterialParameterType::TEXTURE)
        {
            material_shader_info.normal_tex_index = m_material_texture_uris.size();
            m_material_texture_uris.push_back(normal_parameter->GetTexture());
        }
        else
        {
            material_shader_info.normal = normal_parameter->GetFactor();
        }
    }

    if (material.HasParameter(MaterialBase::MaterialParameterUsage::METALLIC_ROUGHNESS) && material.GetParameter(MaterialBase::MaterialParameterUsage::METALLIC_ROUGHNESS))
    {
        auto metallic_roughness_parameter = material.GetParameter(MaterialBase::MaterialParameterUsage::METALLIC_ROUGHNESS);
        if (metallic_roughness_parameter->GetType() == MaterialParameter::MaterialParameterType::TEXTURE)
        {
            material_shader_info.metallic_roughness_tex_index = m_material_texture_uris.size();
            m_material_texture_uris.push_back(metallic_roughness_parameter->GetTexture());
        }
        else
        {
            material_shader_info.metallicAndRoughness = metallic_roughness_parameter->GetFactor();
        }
    }

    m_material_shader_infos[material.GetID()] = material_shader_info;
    return true;
}

bool RendererModuleMaterial::FinalizeModule()
{
    // Upload all texture resource and material shader infos
    for (const auto& texture_uri: m_material_texture_uris)
    {
        RendererInterface::TextureFileDesc material_texture_desc{texture_uri};
        m_material_texture_handles.push_back(m_resource_operator.CreateTexture(material_texture_desc));
    }
    std::vector<MaterialShaderInfo> material_shader_infos{m_material_shader_infos.size()};
    for (size_t i = 0; i < m_material_shader_infos.size(); i++)
    {
        material_shader_infos[i] = m_material_shader_infos[i];
    }
    
    m_material_shader_info_buffer_desc.usage = RendererInterface::USAGE_SRV;
    m_material_shader_info_buffer_desc.size = material_shader_infos.size() * sizeof(MaterialShaderInfo);
    m_material_shader_info_buffer_desc.name = "g_material_infos";
    m_material_shader_info_buffer_desc.type = RendererInterface::DEFAULT;
    m_material_shader_info_buffer_desc.data = material_shader_infos.data();
    m_material_infos_handle = m_resource_operator.CreateBuffer(m_material_shader_info_buffer_desc);

    return true;
}

bool RendererModuleMaterial::BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc)
{
    RendererInterface::BufferBindingDesc binding_desc{};
    binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    binding_desc.buffer_handle = m_material_infos_handle;
    binding_desc.is_structured_buffer = true;
    binding_desc.stride = sizeof(MaterialShaderInfo);
    binding_desc.count = m_material_shader_infos.size();
    out_draw_desc.buffer_resources[m_material_shader_info_buffer_desc.name] = binding_desc;

    RendererInterface::TextureBindingDesc texture_binding_desc{};
    texture_binding_desc.type = RendererInterface::TextureBindingDesc::SRV;
    texture_binding_desc.textures = m_material_texture_handles;
    out_draw_desc.texture_resources["bindless_material_textures"] = texture_binding_desc;
    
    return true;
}

