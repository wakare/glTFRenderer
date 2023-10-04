#pragma once
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHIRootSignatureHelper;

class glTFRenderInterfaceLighting : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceLighting();

    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override;

    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override;

    virtual bool ApplyRootSignature(IRHIRootSignatureHelper& root_signature) override;

    bool UpdateCPUBuffer(const ConstantBufferPerLightDraw& data);
    
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override;
};

