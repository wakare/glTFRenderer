#include "glTFRenderInterfaceSceneMaterial.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceSRVTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"

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