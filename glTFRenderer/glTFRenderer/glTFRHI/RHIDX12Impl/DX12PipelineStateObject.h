#pragma once
#include <d3d12.h>
#include <map>

#include "DX12Shader.h"
#include "../RHIInterface/IRHIPipelineStateObject.h"

class IDX12PipelineStateObjectCommon
{
protected:
    IDX12PipelineStateObjectCommon();
    
    bool CompileBindShaders(const std::map<RHIShaderType, std::shared_ptr<IRHIShader>>& shaders, const RHIShaderPreDefineMacros& shader_macros);
    
    ID3D12PipelineState* m_pipeline_state_object;
};

class DX12GraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DX12GraphicsPipelineStateObject();
    virtual ~DX12GraphicsPipelineStateObject() override;
    
    virtual bool BindRenderTargets(const std::vector<IRHIRenderTarget*>& render_targets) override;
    virtual bool InitGraphicsPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature, IRHISwapChain& swapchain) override;

    ID3D12PipelineState* GetPSO() {return m_pipeline_state_object; }
    
private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_graphics_pipeline_state_desc;
    
    std::vector<DXGI_FORMAT> m_bind_render_target_formats;
    DXGI_FORMAT m_bind_depth_stencil_format;
};

class DX12ComputePipelineStateObject : public IRHIComputePipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DX12ComputePipelineStateObject();

    virtual bool InitComputePipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature) override;
    
private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_compute_pipeline_state_desc;
};