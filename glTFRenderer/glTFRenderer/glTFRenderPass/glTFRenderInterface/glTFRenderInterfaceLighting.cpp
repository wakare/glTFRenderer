#include "glTFRenderInterfaceLighting.h"

#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

bool glTFRenderInterfaceLighting::InitInterface(glTFRenderResourceManager& resource_manager)
{
     RETURN_IF_FALSE(glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>::InitInterface(resource_manager))

     RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<PointLightInfo>::InitInterface(resource_manager))

     RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>::InitInterface(resource_manager))
     
     return true;
}

bool glTFRenderInterfaceLighting::ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline)
{
     RETURN_IF_FALSE(glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>::ApplyInterface(resource_manager, isGraphicsPipeline))

     RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<PointLightInfo>::ApplyInterface(resource_manager, isGraphicsPipeline))

     RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>::ApplyInterface(resource_manager, isGraphicsPipeline))
    
     return true;
}

bool glTFRenderInterfaceLighting::ApplyRootSignature(IRHIRootSignatureHelper& root_signature)
{
     RETURN_IF_FALSE(glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>::ApplyRootSignature(root_signature))

     RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<PointLightInfo>::ApplyRootSignature(root_signature))

     RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>::ApplyRootSignature(root_signature))
    
     return true;
}

bool glTFRenderInterfaceLighting::UpdateCPUBuffer(const ConstantBufferPerLightDraw& data)
{
     RETURN_IF_FALSE(glTFRenderInterfaceSingleConstantBuffer::UpdateCPUBuffer(&data.light_info, sizeof(data.light_info)))
     
     if (!data.point_light_infos.empty())
     {
          RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<PointLightInfo>::UpdateCPUBuffer(data.point_light_infos.data(), data.point_light_infos.size() * sizeof(PointLightInfo)))
     }

     if (!data.directional_light_infos.empty())
     {
          RETURN_IF_FALSE(glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>::UpdateCPUBuffer(data.directional_light_infos.data(), data.directional_light_infos.size() * sizeof(DirectionalLightInfo)))
     }

     return true;
}

void glTFRenderInterfaceLighting::UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
     // Update light info shader define
     char registerIndexValue[16] = {'\0'};
    
     (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", glTFRenderInterfaceSingleConstantBuffer::m_allocation.register_index);
     outShaderPreDefineMacros.AddMacro("SCENE_LIGHT_LIGHT_INFO_REGISTER_INDEX", registerIndexValue);
    
     (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", glTFRenderInterfaceStructuredBuffer<PointLightInfo>::m_allocation.register_index);
     outShaderPreDefineMacros.AddMacro("SCENE_LIGHT_POINT_LIGHT_REGISTER_INDEX", registerIndexValue);

     (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>::m_allocation.register_index);
     outShaderPreDefineMacros.AddMacro("SCENE_LIGHT_DIRECTIONAL_LIGHT_REGISTER_INDEX", registerIndexValue);
}
