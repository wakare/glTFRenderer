#include "glTFRenderInterfaceLighting.h"

#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

glTFRenderInterfaceLighting::glTFRenderInterfaceLighting()
{
     AddInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>>());
     AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<PointLightInfo>>());
     AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>>());
}

bool glTFRenderInterfaceLighting::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
     return true;
}

bool glTFRenderInterfaceLighting::ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline)
{
     return true;
}

bool glTFRenderInterfaceLighting::ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature)
{
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

void glTFRenderInterfaceLighting::ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const
{
}
