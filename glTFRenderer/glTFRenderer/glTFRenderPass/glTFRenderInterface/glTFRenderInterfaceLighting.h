#pragma once
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"

class glTFRenderInterfaceLighting
    : public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>
    , public glTFRenderInterfaceStructuredBuffer<PointLightInfo>
    , public glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>
{
public:
    glTFRenderInterfaceLighting(unsigned root_parameter_cbv_index, unsigned register_index,
        unsigned point_light_buffer_index, unsigned point_light_buffer_register_index, unsigned directional_light_structured_buffer_index,
        unsigned directional_light_register_index)
        : glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>(root_parameter_cbv_index, register_index),
          glTFRenderInterfaceStructuredBuffer<PointLightInfo>(point_light_buffer_index, point_light_buffer_register_index),
          glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>(directional_light_structured_buffer_index, directional_light_register_index)
    {
    }

    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override;

    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override;

    virtual bool ApplyRootSignature(IRHIRootSignature& root_signature) const override;

    bool UpdateCPUBuffer(const ConstantBufferPerLightDraw& data);
    
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};

