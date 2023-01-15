#pragma once
#include "IRHIDevice.h"
#include "IRHIRootSignature.h"
#include "IRHIResource.h"
#include "IRHIShader.h"

enum class RHIPipelineType
{
    Graphics,
    Compute,
    //Copy,
    Unknown,
};

class IRHIPipelineStateObject : public IRHIResource
{
public:
    IRHIPipelineStateObject(RHIPipelineType type);
    virtual bool CompileAndSetShaderCode(const IRHIShader& shader) = 0;
    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& rootSignature) = 0;

protected:
    RHIPipelineType m_type;
};
