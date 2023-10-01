#pragma once
#include "DX12Common.h"
#include <map>

#include "DX12Shader.h"
#include "../RHIInterface/IRHIPipelineStateObject.h"
#include "glTFUtils/glTFUtils.h"

class IDX12PipelineStateObjectCommon
{
public:
    ID3D12PipelineState* GetPipelineStateObject() const { return m_pipeline_state_object.Get(); }
    ID3D12StateObject* GetDXRPipelineStateObject() const { return m_dxr_pipeline_state.Get(); }
    
protected:
    IDX12PipelineStateObjectCommon();
    
    bool CompileBindShaders(const std::map<RHIShaderType, std::shared_ptr<IRHIShader>>& shaders, const RHIShaderPreDefineMacros& shader_macros);
    
    ComPtr<ID3D12PipelineState> m_pipeline_state_object;
    ComPtr<ID3D12StateObject> m_dxr_pipeline_state;
};

class DX12GraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DX12GraphicsPipelineStateObject();
    virtual ~DX12GraphicsPipelineStateObject() override;

    virtual bool BindSwapChain(const IRHISwapChain& swapchain) override;
    virtual bool BindRenderTargets(const std::vector<IRHIRenderTarget*>& render_targets) override;
    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature) override;

    ID3D12PipelineState* GetPSO() {return m_pipeline_state_object.Get(); }
    
private:
    std::vector<DXGI_FORMAT> m_bind_render_target_formats;
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_graphics_pipeline_state_desc;
    DXGI_FORMAT m_bind_depth_stencil_format;
    DXGI_SAMPLE_DESC m_swapchain_sample_desc;
};

class DX12ComputePipelineStateObject : public IRHIComputePipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DX12ComputePipelineStateObject();

    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature) override;
    
private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_compute_pipeline_state_desc;
};

class DX12DXRStateObject : public IRHIRayTracingPipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DX12DXRStateObject();
    virtual bool InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature) override;
    
    ID3D12StateObjectProperties* GetDXRStateObjectProperties()
    {
        if (!m_dxr_pipeline_state_properties.Get())
        {
            GLTF_CHECK(m_dxr_pipeline_state.Get());

            THROW_IF_FAILED(m_dxr_pipeline_state.As<ID3D12StateObjectProperties>(&m_dxr_pipeline_state_properties))
            GLTF_CHECK(m_dxr_pipeline_state_properties.Get());
        }

        return m_dxr_pipeline_state_properties.Get();
    }
    
protected:
    ComPtr<ID3D12StateObjectProperties> m_dxr_pipeline_state_properties;
    CD3DX12_STATE_OBJECT_DESC m_dxr_state_desc;
};