#include "DX12PipelineStateObject.h"
#include "d3dx12.h"

#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "DX12Shader.h"
#include "DX12SwapChain.h"
#include "DX12Utils.h"
#include "../RHIResourceFactoryImpl.hpp"

IDX12PipelineStateObjectCommon::IDX12PipelineStateObjectCommon()
    : m_pipeline_state_object(nullptr)
{
}

bool IDX12PipelineStateObjectCommon::CompileBindShaders(const std::map<RHIShaderType, std::shared_ptr<IRHIShader>>& shaders, const RHIShaderPreDefineMacros& shader_macros)
{
    RETURN_IF_FALSE(!shaders.empty())

    for (const auto& shader : shaders)
    {
        shader.second->SetShaderCompilePreDefineMacros(shader_macros);
        RETURN_IF_FALSE(shader.second->CompileShader())
    }

    return true;
}

DX12GraphicsPipelineStateObject::DX12GraphicsPipelineStateObject()
    : m_graphics_pipeline_state_desc({})
    , m_bind_depth_stencil_format(DXGI_FORMAT_UNKNOWN)
{
}

DX12GraphicsPipelineStateObject::~DX12GraphicsPipelineStateObject()
{
    SAFE_RELEASE(m_pipeline_state_object)
}

bool DX12GraphicsPipelineStateObject::BindRenderTargets(const std::vector<IRHIRenderTarget*>& render_targets)
{
    m_bind_render_target_formats.clear();
    for (const auto& render_target : render_targets)
    {
        if (render_target->GetRenderTargetType() == RHIRenderTargetType::RTV)
        {
            m_bind_render_target_formats.push_back(DX12ConverterUtils::ConvertToDXGIFormat(render_target->GetRenderTargetFormat()));    
        }
        else if (render_target->GetRenderTargetType() == RHIRenderTargetType::DSV)
        {
            const RHIDataFormat convert_depth_stencil_format = render_target->GetRenderTargetFormat() == RHIDataFormat::R32_TYPELESS ?
                RHIDataFormat::D32_FLOAT : render_target->GetRenderTargetFormat();
            m_bind_depth_stencil_format = DX12ConverterUtils::ConvertToDXGIFormat(convert_depth_stencil_format);
        }
        else
        {
            // Not supported render target type!
            assert(false);
        }
    }
    
    return true;
}

bool DX12GraphicsPipelineStateObject::InitGraphicsPipelineStateObject(IRHIDevice& device, IRHIRootSignature& root_signature, IRHISwapChain& swapchain)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxRootSignature = dynamic_cast<DX12RootSignature&>(root_signature).GetRootSignature();
    auto& dxSwapchain = dynamic_cast<DX12SwapChain&>(swapchain);
    
    // create input layout

    // The input layout is used by the Input Assembler so that it knows
    // how to read the vertex data bound to it.

    std::vector<D3D12_INPUT_ELEMENT_DESC> dx_input_layouts;
    dx_input_layouts.reserve(m_input_layouts.size());
    
    for (const auto& input_layout : m_input_layouts)
    {
        dx_input_layouts.push_back({input_layout.semanticName.c_str(), input_layout.semanticIndex, DX12ConverterUtils::ConvertToDXGIFormat(input_layout.format),
            0, input_layout.alignedByteOffset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});  
    }
    
    // fill out an input layout description structure
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

    // we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
    inputLayoutDesc.NumElements = dx_input_layouts.size();
    inputLayoutDesc.pInputElementDescs = dx_input_layouts.data();

    // compile shader and set bytecode to pipeline state desc
    THROW_IF_FAILED(CompileBindShaders(m_shaders, m_shader_macros))
    D3D12_SHADER_BYTECODE vertexShaderBytecode;
    {
        auto& bindVS = dynamic_cast<DX12Shader&>(GetBindShader(RHIShaderType::Vertex));
    
        vertexShaderBytecode.BytecodeLength = bindVS.GetShaderByteCode().size();
        vertexShaderBytecode.pShaderBytecode = bindVS.GetShaderByteCode().data();    
    }

    D3D12_SHADER_BYTECODE pixelShaderBytecode;
    if (m_depth_stencil_state != IRHIDepthStencilMode::DEPTH_WRITE)
    {
        auto& bindPS = dynamic_cast<DX12Shader&>(GetBindShader(RHIShaderType::Pixel));
        pixelShaderBytecode.BytecodeLength = bindPS.GetShaderByteCode().size();
        pixelShaderBytecode.pShaderBytecode = bindPS.GetShaderByteCode().data();    
    }
    
    m_graphics_pipeline_state_desc.InputLayout = inputLayoutDesc; // the structure describing our input layout
    m_graphics_pipeline_state_desc.pRootSignature = dxRootSignature; // the root signature that describes the input data this pso needs
    m_graphics_pipeline_state_desc.VS = vertexShaderBytecode; // structure describing where to find the vertex shader bytecode and how large it is

    // depth pass do not need pixel shader
    if (m_depth_stencil_state != IRHIDepthStencilMode::DEPTH_WRITE)
    {
        m_graphics_pipeline_state_desc.PS = pixelShaderBytecode; // same as VS but for pixel shader    
    }
    
    m_graphics_pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing

    {
        unsigned index = 0;
        for (const auto& format : m_bind_render_target_formats)
        {
            m_graphics_pipeline_state_desc.RTVFormats[index++] = format;    
        }
    }
    
    m_graphics_pipeline_state_desc.DSVFormat = m_bind_depth_stencil_format;
    
    m_graphics_pipeline_state_desc.SampleDesc = dxSwapchain.GetSwapChainSampleDesc(); // must be the same sample description as the swapchain and depth/stencil buffer
    m_graphics_pipeline_state_desc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
    m_graphics_pipeline_state_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
    switch (m_cullMode) {
        case IRHICullMode::NONE:
            {
                m_graphics_pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;    
            }
            break;
        
        case IRHICullMode::CW:
            {
                m_graphics_pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;     
            }
            break;
        
        case IRHICullMode::CCW:
            {
                m_graphics_pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT;     
            }
        
            break;
        }
    
    m_graphics_pipeline_state_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
    m_graphics_pipeline_state_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); 
    switch (m_depth_stencil_state) {
    case IRHIDepthStencilMode::DEPTH_READ:
        {
            m_graphics_pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_EQUAL;
            m_graphics_pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ZERO;
        }
        break;
    case IRHIDepthStencilMode::DEPTH_WRITE:
        { 
        }
        break;
    default:
        { 
        };
    } 
    m_graphics_pipeline_state_desc.NumRenderTargets = m_bind_render_target_formats.size();

    THROW_IF_FAILED(dxDevice->CreateGraphicsPipelineState(&m_graphics_pipeline_state_desc, IID_PPV_ARGS(&m_pipeline_state_object)))
    
    return true;
}

DX12ComputePipelineStateObject::DX12ComputePipelineStateObject()
    : m_compute_pipeline_state_desc({})
{
}

bool DX12ComputePipelineStateObject::InitComputePipelineStateObject(IRHIDevice& device,
    IRHIRootSignature& root_signature)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxRootSignature = dynamic_cast<DX12RootSignature&>(root_signature).GetRootSignature();

    RETURN_IF_FALSE(CompileBindShaders(m_shaders, m_shader_macros))
    D3D12_SHADER_BYTECODE compute_shader_bytecode;
    {
        const auto& bindCS = dynamic_cast<DX12Shader&>(GetBindShader(RHIShaderType::Compute));
        compute_shader_bytecode.BytecodeLength = bindCS.GetShaderByteCode().size();
        compute_shader_bytecode.pShaderBytecode = bindCS.GetShaderByteCode().data();    
    }
    
    m_compute_pipeline_state_desc.pRootSignature = dxRootSignature;
    m_compute_pipeline_state_desc.CS = compute_shader_bytecode;
    m_compute_pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    m_compute_pipeline_state_desc.NodeMask = 0;
    m_compute_pipeline_state_desc.CachedPSO.pCachedBlob = nullptr;
    m_compute_pipeline_state_desc.CachedPSO.CachedBlobSizeInBytes = 0;

    THROW_IF_FAILED(dxDevice->CreateComputePipelineState(&m_compute_pipeline_state_desc, IID_PPV_ARGS(&m_pipeline_state_object)))
    
    return true;
}