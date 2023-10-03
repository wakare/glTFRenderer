#pragma once
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHIRootSignatureHelper;

class glTFRenderInterfaceLighting
    : public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>
    , public glTFRenderInterfaceStructuredBuffer<PointLightInfo>
    , public glTFRenderInterfaceStructuredBuffer<DirectionalLightInfo>
{
public:
    glTFRenderInterfaceLighting() = default;

    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override;

    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override;

    virtual bool ApplyRootSignature(IRHIRootSignatureHelper& root_signature);

    bool UpdateCPUBuffer(const ConstantBufferPerLightDraw& data);
    
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};

