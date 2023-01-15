#include "DX12PipelineStateObject.h"

#include "DX12Shader.h"

DX12GraphicsPipelineStateObject::DX12GraphicsPipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::Graphics)
    , m_graphicsPipelineStateDesc({})
{
}

bool DX12GraphicsPipelineStateObject::CompileAndSetShaderCode(const IRHIShader& shader)
{
    
    
    return true;
}

DX12ComputePipelineStateObject::DX12ComputePipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::Compute)
    , m_computePipelineStateDesc({})
{
}
