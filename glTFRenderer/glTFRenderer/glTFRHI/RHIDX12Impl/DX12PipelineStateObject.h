#pragma once
#include "DX12Common.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "RendererCommon.h"

class IDX12PipelineStateObjectCommon
{
public:
    ID3D12PipelineState* GetPipelineStateObject() const { return m_pipeline_state_object.Get(); }
    ID3D12StateObject* GetDXRPipelineStateObject() const { return m_dxr_pipeline_state.Get(); }

protected:
    IDX12PipelineStateObjectCommon();
    
    ComPtr<ID3D12PipelineState> m_pipeline_state_object;
    ComPtr<ID3D12StateObject> m_dxr_pipeline_state;
};

class DX12GraphicsPipelineStateObject : public IRHIGraphicsPipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12GraphicsPipelineStateObject)
    
    virtual bool BindRenderTargetFormats(const std::vector<IRHIDescriptorAllocation*>& render_targets) override;
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain) override;

    ID3D12PipelineState* GetPSO() {return m_pipeline_state_object.Get(); }

    virtual bool Release(glTFRenderResourceManager&) override;
    
private:
    std::vector<DXGI_FORMAT> m_bind_render_target_formats;
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_graphics_pipeline_state_desc{};
    DXGI_FORMAT m_bind_depth_stencil_format{DXGI_FORMAT_UNKNOWN};
};

class DX12ComputePipelineStateObject : public IRHIComputePipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DX12ComputePipelineStateObject();

    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain) override;
    virtual bool Release(glTFRenderResourceManager&) override;
    
private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_compute_pipeline_state_desc;
};

class DX12RTPipelineStateObject : public IRHIRayTracingPipelineStateObject, public IDX12PipelineStateObjectCommon
{
public:
    DX12RTPipelineStateObject();
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain) override;
    virtual bool Release(glTFRenderResourceManager&) override;
    
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