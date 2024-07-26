#include "glTFRenderInterfaceSceneMaterial.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceSRVTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderPass/glTFRenderMaterialManager.h"

glTFRenderInterfaceSceneMaterial::glTFRenderInterfaceSceneMaterial()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>(MaterialInfo::Name.c_str()));
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTableBindless>("SCENE_MATERIAL_TEXTURE_REGISTER_INDEX"));

    std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>("SCENE_MATERIAL_SAMPLER_REGISTER_INDEX");
    AddInterface(sampler_interface);
}

bool glTFRenderInterfaceSceneMaterial::UploadMaterialData(glTFRenderResourceManager& resource_manager)
{
    std::vector<MaterialInfo> material_infos;
    std::vector<glTFMaterialTextureRenderResource*> material_texture_render_resources;
    RETURN_IF_FALSE(resource_manager.GetMaterialManager().GatherAllMaterialRenderResource(material_infos, material_texture_render_resources))

    // Create all material texture into specific heap
    RHIGPUDescriptorHandle handle = UINT64_MAX;

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

    // Set heap handle to bindless parameter slot
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>()->UploadCPUBuffer(resource_manager, material_infos.data(), 0, sizeof(MaterialInfo) * material_infos.size()))
    GetRenderInterface<glTFRenderInterfaceSRVTableBindless>()->AddTextureAllocations(texture_descriptor_allocations);
    
    return true;
}

bool glTFRenderInterfaceSceneMaterial::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderInterfaceSceneMaterial::ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type,
    IRHIDescriptorUpdater& descriptor_updater)
{
    return true;
}

bool glTFRenderInterfaceSceneMaterial::ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature)
{
    return true;
}

void glTFRenderInterfaceSceneMaterial::ApplyShaderDefineImpl(
    RHIShaderPreDefineMacros& out_shader_pre_define_macros) const
{
}
