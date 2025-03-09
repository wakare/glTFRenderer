#include "glTFRenderInterfaceSceneMaterial.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceSRVTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderInterfaceVT.h"
#include "glTFRenderPass/glTFRenderMaterialManager.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"

glTFRenderInterfaceSceneMaterial::glTFRenderInterfaceSceneMaterial()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>(MaterialInfo::Name.c_str()));
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTableBindless>("SCENE_MATERIAL_TEXTURE_REGISTER_INDEX"));
    AddInterface(std::make_shared<glTFRenderInterfaceVT>());
    
    std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>("SCENE_MATERIAL_SAMPLER_REGISTER_INDEX");
    AddInterface(sampler_interface);
}

bool glTFRenderInterfaceSceneMaterial::PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    const auto& material_resources = resource_manager.GetMaterialManager().GetMaterialRenderResources();
    std::vector<MaterialInfo> material_infos; material_infos.resize(material_resources.size());
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> texture_descriptor_allocations;
    
    for (const auto& material_resource : material_resources)
    {
        unsigned material_id = material_resource.first;
        auto& material_info = material_infos[material_id];
        
        const auto& material_render_resource = material_resource.second;
        const auto& material_factor = material_render_resource->GetFactors();
        const auto& material_render_textures = material_render_resource->GetTextures();

        material_info.albedo = material_factor.contains(glTFMaterialParameterUsage::BASECOLOR) ?
            material_factor.at(glTFMaterialParameterUsage::BASECOLOR)->GetFactor(): glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        material_info.normal = material_factor.contains(glTFMaterialParameterUsage::NORMAL) ?
            material_factor.at(glTFMaterialParameterUsage::NORMAL)->GetFactor(): glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        material_info.metallicAndRoughness = material_factor.contains(glTFMaterialParameterUsage::METALLIC_ROUGHNESS) ?
            material_factor.at(glTFMaterialParameterUsage::METALLIC_ROUGHNESS)->GetFactor(): glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

        auto process_texture = [&, this]( glTFMaterialParameterUsage usage, unsigned& out_texture_index,
            unsigned& out_vt_flags)
        {
            if (material_render_textures.contains(usage))
            {
                const auto& texture = material_render_textures.at(usage);
                if (texture->IsVT())
                {
                    auto virtual_texture = texture->GetVTTexture();
                    resource_manager.GetRenderSystem<VirtualTextureSystem>()->RegisterTexture(virtual_texture);
                    
                    GetRenderInterface<glTFRenderInterfaceVT>()->AddVirtualTexture(virtual_texture);
                    out_texture_index = virtual_texture->GetTextureId();
                    switch (usage) {
                    case glTFMaterialParameterUsage::BASECOLOR:
                        out_vt_flags |= MaterialInfo::VT_FLAG_ALBEDO;
                        break;
                    case glTFMaterialParameterUsage::NORMAL:
                        out_vt_flags |= MaterialInfo::VT_FLAG_NORMAL;
                        break;
                    case glTFMaterialParameterUsage::METALLIC_ROUGHNESS:
                        out_vt_flags |= MaterialInfo::VT_FLAG_SPECULAR;
                        break;
                    }
                }
                else
                {
                    std::shared_ptr<IRHITextureDescriptorAllocation> result;
                    auto& texture_resource = texture->GetTextureAllocation().m_texture;
                    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture_resource,
                        RHITextureDescriptorDesc{
                            texture_resource->GetTextureDesc().GetDataFormat(),
                            RHIResourceDimension::TEXTURE2D,
                            RHIViewType::RVT_SRV,
                        },
                        result);
                    out_texture_index = texture_descriptor_allocations.size();
                    texture_descriptor_allocations.push_back(result);
                }
            }
        };

        process_texture(glTFMaterialParameterUsage::BASECOLOR, material_info.albedo_tex_index, material_info.vt_flags);
        process_texture(glTFMaterialParameterUsage::NORMAL, material_info.normal_tex_index, material_info.vt_flags);
        process_texture(glTFMaterialParameterUsage::METALLIC_ROUGHNESS, material_info.metallic_roughness_tex_index, material_info.vt_flags);

        RETURN_IF_FALSE(resource_manager.GetRenderSystem<VirtualTextureSystem>()->InitRenderResource(resource_manager));
    }
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>()->UploadCPUBuffer(resource_manager, material_infos.data(), 0, sizeof(MaterialInfo) * material_infos.size()))

    RETURN_IF_FALSE(glTFRenderInterfaceBase::PostInitInterfaceImpl(resource_manager))

    return true;
}