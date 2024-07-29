#include "RHICommon.h"

#include "glTFRHI/RHIConfigSingleton.h"
#include "glTFRHI/RHIUtils.h"

void RootSignatureAllocation::AddShaderDefine(RHIShaderPreDefineMacros& out_shader_macros) const
{
    char shader_resource_declaration[64] = {'\0'};

    std::string register_name;
    switch (register_type) {
    case RHIShaderRegisterType::b:
        register_name = "b";
        break;
    case RHIShaderRegisterType::t:
        register_name = "t";
        break;
    case RHIShaderRegisterType::u:
        register_name = "u";
        break;
    case RHIShaderRegisterType::s:
        register_name = "s";
        break;
    case RHIShaderRegisterType::Unknown:
        GLTF_CHECK(false);
        break;
    }

    
    if (RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12)
    {
        memset(shader_resource_declaration, 0, sizeof(shader_resource_declaration));
        (void)snprintf(shader_resource_declaration, sizeof(shader_resource_declaration), "register(%s%d, space%u)", register_name.c_str(), register_index, space);
        out_shader_macros.AddMacro(parameter_name, shader_resource_declaration);    
    }
    else
    {
        memset(shader_resource_declaration, 0, sizeof(shader_resource_declaration));
        // VK layout shader macro
        (void)snprintf(shader_resource_declaration, sizeof(shader_resource_declaration), "[[vk::binding(%d, %d)]]", register_index, space);
        out_shader_macros.AddMacro(parameter_name, shader_resource_declaration);
    }
    out_shader_macros.AddMacro("DX_SHADER", RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12 ? "1" : "0");
}
