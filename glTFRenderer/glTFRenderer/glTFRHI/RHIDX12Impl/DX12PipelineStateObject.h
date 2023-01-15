#pragma once
#include <d3d12.h>
#include <map>

#include "../RHIInterface/IRHIPipelineStateObject.h"

class DX12GraphicsPipelineStateObject : public IRHIPipelineStateObject
{
public:
    DX12GraphicsPipelineStateObject();
    virtual bool CompileAndSetShaderCode(const IRHIShader& shader) override;
    
private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_graphicsPipelineStateDesc;

    std::map<RHIShaderType, >
};

// TODO: Implement in the future!
class DX12ComputePipelineStateObject : public IRHIPipelineStateObject
{
public:
    DX12ComputePipelineStateObject();

private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_computePipelineStateDesc;
};