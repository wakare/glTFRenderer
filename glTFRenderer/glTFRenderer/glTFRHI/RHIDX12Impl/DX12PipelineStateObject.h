#pragma once
#include <d3d12.h>
#include <map>

#include "DX12Shader.h"
#include "../RHIInterface/IRHIPipelineStateObject.h"

class DX12GraphicsPipelineStateObject : public IRHIPipelineStateObject
{
public:
    DX12GraphicsPipelineStateObject();
    virtual bool BindShaderCode(const std::string& shaderFilePath, RHIShaderType type) override;
    virtual bool BindRenderTargets(const std::vector<IRHIRenderTarget*>& renderTargets) override;
    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& rootSignature, IRHISwapChain& swapchain, const std::vector<RHIPipelineInputLayout>& inputLayouts) override;

    virtual IRHIShader& GetBindShader(RHIShaderType type) override;
    ID3D12PipelineState* GetPSO() {return m_pipelineStateObject; }
    
private:
    bool CompileBindShaders();
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_graphicsPipelineStateDesc;
    std::map<RHIShaderType, std::shared_ptr<IRHIShader>> m_shaders;
    
    std::vector<DXGI_FORMAT> m_bindRenderTargetFormats;
    DXGI_FORMAT m_bindDepthStencilFormat;

    ID3D12PipelineState* m_pipelineStateObject;
};

// TODO: Implement in the future!
class DX12ComputePipelineStateObject : public IRHIPipelineStateObject
{
public:
    DX12ComputePipelineStateObject();

private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_computePipelineStateDesc;
};