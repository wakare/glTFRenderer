#include "glTFRenderInterfaceSceneMaterial.h"

#include "glTFRenderInterfaceSRVTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"

glTFRenderInterfaceSceneMaterial::glTFRenderInterfaceSceneMaterial()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MaterialInfo>>());
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTableBindless>());

    GetRenderInterface<glTFRenderInterfaceSRVTableBindless>()->SetSRVRegisterNames({"SCENE_MATERIAL_TEXTURE_REGISTER_INDEX"});
}

bool glTFRenderInterfaceSceneMaterial::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderInterfaceSceneMaterial::ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager,
    bool isGraphicsPipeline)
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
