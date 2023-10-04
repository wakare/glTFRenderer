#include "glTFRenderInterfaceLighting.h"

#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

glTFRenderInterfaceLighting::glTFRenderInterfaceLighting()
{
     AddInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>>());
     AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<PointLightInfo>>());
     AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>>());
}

bool glTFRenderInterfaceLighting::InitInterface(glTFRenderResourceManager& resource_manager)
{
     for (const auto& sub_interface : m_sub_interfaces)
     {
          RETURN_IF_FALSE(sub_interface->InitInterface(resource_manager))     
     }
     
     return true;
}

bool glTFRenderInterfaceLighting::ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline)
{
     for (const auto& sub_interface : m_sub_interfaces)
     {
          RETURN_IF_FALSE(sub_interface->ApplyInterface(resource_manager, isGraphicsPipeline))     
     }
     
     return true;
}

bool glTFRenderInterfaceLighting::ApplyRootSignature(IRHIRootSignatureHelper& root_signature)
{
     for (const auto& sub_interface : m_sub_interfaces)
     {
          RETURN_IF_FALSE(sub_interface->ApplyRootSignature(root_signature))     
     }
     
     return true;
}

bool glTFRenderInterfaceLighting::UpdateCPUBuffer(const ConstantBufferPerLightDraw& data)
{
     RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>>()->UpdateCPUBuffer(&data.light_info, sizeof(data.light_info)))
     
     if (!data.point_light_infos.empty())
     {
          RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<PointLightInfo>>()->UpdateCPUBuffer(data.point_light_infos.data(), data.point_light_infos.size() * sizeof(PointLightInfo)))
     }

     if (!data.directional_light_infos.empty())
     {
          RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>>()->UpdateCPUBuffer(data.directional_light_infos.data(), data.directional_light_infos.size() * sizeof(DirectionalLightInfo)))
     }

     return true;
}

void glTFRenderInterfaceLighting::UpdateShaderCompileDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const
{
     for (const auto& sub_interface : m_sub_interfaces)
     {
          sub_interface->UpdateShaderCompileDefine(out_shader_pre_define_macros);
     }
}
