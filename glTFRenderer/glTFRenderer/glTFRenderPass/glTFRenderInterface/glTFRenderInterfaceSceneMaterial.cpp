#include "glTFRenderInterfaceSceneMaterial.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceSRVTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderInterfaceVT.h"
#include "glTFRenderPass/glTFRenderMaterialManager.h"

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
    RETURN_IF_FALSE(glTFRenderInterfaceBase::PostInitInterfaceImpl(resource_manager))

    std::vector<MaterialInfo> material_infos;
    std::vector<const glTFMaterialTextureRenderResource*> material_texture_render_resources;
    std::vector<const VTLogicalTexture*> virtual_textures;
    RETURN_IF_FALSE(resource_manager.GetMaterialManager().GatherAllMaterialRenderResource(material_infos, material_texture_render_resources, virtual_textures))

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>()->UploadCPUBuffer(resource_manager, material_infos.data(), 0, sizeof(MaterialInfo) * material_infos.size()))
    
    {
        std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> texture_descriptor_allocations;
        for (auto& texture : material_texture_render_resources)
        {
            if (!texture)
            {
                continue;
            }
        
            std::shared_ptr<IRHITextureDescriptorAllocation> result;
            auto& texture_resource = texture->GetTextureAllocation().m_texture;
            resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture_resource,
                RHITextureDescriptorDesc{
                    texture_resource->GetTextureDesc().GetDataFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_SRV,
                },
                result);
            texture_descriptor_allocations.push_back(result);
        }
    
        GetRenderInterface<glTFRenderInterfaceSRVTableBindless>()->AddTextureAllocations(texture_descriptor_allocations);    
    }
    
    for (const auto* virtual_texture : virtual_textures)
    {
        
    }

    return true;
}