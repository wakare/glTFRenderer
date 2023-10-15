#pragma once
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHIRootSignatureHelper;

class glTFRenderInterfaceLighting : public glTFRenderInterfaceBaseWithDefaultImpl
{
public:
    glTFRenderInterfaceLighting();
    
    bool UpdateCPUBuffer();
    bool UpdateLightInfo(const glTFLightBase& light);

protected:
    ConstantBufferPerLightDraw m_light_buffer_data;
};

