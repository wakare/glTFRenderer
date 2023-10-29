#include "glTFRenderInterfaceSceneMaterial.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceSRVTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderPass/glTFRenderMaterialManager.h"

glTFRenderInterfaceSceneMaterial::glTFRenderInterfaceSceneMaterial()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>());
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTableBindless>());

    GetRenderInterface<glTFRenderInterfaceSRVTableBindless>()->SetSRVRegisterNames({"SCENE_MATERIAL_TEXTURE_REGISTER_INDEX"});
    std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>();
    sampler_interface->SetSamplerRegisterIndexName("SCENE_MATERIAL_SAMPLER_REGISTER_INDEX");
    AddInterface(sampler_interface);
}

bool glTFRenderInterfaceSceneMaterial::UploadMaterialData(glTFRenderResourceManager& resource_manager,
                                                          IRHIDescriptorHeap& heap)
{
    std::vector<MaterialInfo> material_infos;
    std::vector<glTFMaterialTextureRenderResource*> material_texture_render_resources;
    RETURN_IF_FALSE(resource_manager.GetMaterialManager().GatherAllMaterialRenderResource(material_infos, material_texture_render_resources))

    // Create all material texture into specific heap
    RHIGPUDescriptorHandle handle = 0;
    for (auto& texture : material_texture_render_resources)
    {
        auto new_handle = texture->CreateTextureSRVHandleInHeap(resource_manager, heap);
        handle = handle ? handle : new_handle;
    }

    if (!handle)
    {
        return false;
    }
    
    // Set heap handle to bindless parameter slot
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>()->UploadCPUBuffer(material_infos.data(), sizeof(MaterialInfo) * material_infos.size()))
    GetRenderInterface<glTFRenderInterfaceSRVTableBindless>()->SetGPUHandle(handle);

    return true;
}

bool glTFRenderInterfaceSceneMaterial::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderInterfaceSceneMaterial::ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager,
    bool is_graphics_pipeline)
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
