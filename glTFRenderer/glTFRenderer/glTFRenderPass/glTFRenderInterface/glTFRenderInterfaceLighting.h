#pragma once
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHIRootSignatureHelper;

class glTFRenderInterfaceLighting : public glTFRenderInterfaceBaseWithDefaultImpl
{
public:
    glTFRenderInterfaceLighting();
    bool UpdateCPUBuffer(const ConstantBufferPerLightDraw& data);
};

