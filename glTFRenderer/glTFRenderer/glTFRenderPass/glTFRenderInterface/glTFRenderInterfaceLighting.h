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
        unsigned structured_buffer_index, unsigned register_index1, unsigned structured_buffer_index1,
        unsigned register_index2)
        : glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>(root_parameter_cbv_index, register_index),
          glTFRenderInterfaceStructuredBuffer<PointLightInfo>(structured_buffer_index, register_index1),
          glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>(structured_buffer_index1, register_index2)
    {
    }

    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override;

    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager) override;

    virtual bool ApplyRootSignature(IRHIRootSignature& root_signature) const override;

    bool UpdateCPUBuffer(const ConstantBufferPerLightDraw& data);
    
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};

