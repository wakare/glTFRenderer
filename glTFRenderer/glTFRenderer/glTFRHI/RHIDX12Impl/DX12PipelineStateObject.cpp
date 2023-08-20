#include "DX12PipelineStateObject.h"

#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "DX12Shader.h"
#include "DX12SwapChain.h"
#include "DX12Utils.h"
#include "../RHIResourceFactoryImpl.hpp"
#include "d3dx12.h"

DX12GraphicsPipelineStateObject::DX12GraphicsPipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::Graphics)
    , m_graphicsPipelineStateDesc({})
    , m_bindDepthStencilFormat(DXGI_FORMAT_UNKNOWN)
    , m_pipelineStateObject(nullptr)
{
}

DX12GraphicsPipelineStateObject::~DX12GraphicsPipelineStateObject()
{
    SAFE_RELEASE(m_pipelineStateObject)
}

bool DX12GraphicsPipelineStateObject::BindShaderCode(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName)
{
    std::shared_ptr<IRHIShader> dxShader = RHIResourceFactory::CreateRHIResource<IRHIShader>();
    if (!dxShader->InitShader(shaderFilePath, type, entryFunctionName))
    {
        return false;
    }

    // Delay compile shader bytecode util create pso

    assert(m_shaders.find(type) == m_shaders.end());
    m_shaders[type] = dxShader;
    
    return true;
}

bool DX12GraphicsPipelineStateObject::BindRenderTargets(const std::vector<IRHIRenderTarget*>& renderTargets)
{
    m_bindRenderTargetFormats.clear();
    for (const auto& renderTarget : renderTargets)
    {
        if (renderTarget->GetRenderTargetType() == RHIRenderTargetType::RTV)
        {
            m_bindRenderTargetFormats.push_back(DX12ConverterUtils::ConvertToDXGIFormat(renderTarget->GetRenderTargetFormat()));    
        }
        else if (renderTarget->GetRenderTargetType() == RHIRenderTargetType::DSV)
        {
            const RHIDataFormat convertDepthStencilFormat = renderTarget->GetRenderTargetFormat() == RHIDataFormat::R32_TYPELESS ?
                RHIDataFormat::D32_FLOAT : renderTarget->GetRenderTargetFormat();
            m_bindDepthStencilFormat = DX12ConverterUtils::ConvertToDXGIFormat(convertDepthStencilFormat);
        }
        else
        {
            // Not supported render target type!
            assert(false);
        }
    }
    
    return true;
}

bool DX12GraphicsPipelineStateObject::InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& rootSignature, IRHISwapChain& swapchain)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxRootSignature = dynamic_cast<DX12RootSignature&>(rootSignature).GetRootSignature();
    auto& dxSwapchain = dynamic_cast<DX12SwapChain&>(swapchain);
    
    // create input layout

    // The input layout is used by the Input Assembler so that it knows
    // how to read the vertex data bound to it.

    std::vector<D3D12_INPUT_ELEMENT_DESC> dxInputLayouts;
    for (const auto& inputLayout : m_input_layouts)
    {
        dxInputLayouts.push_back({inputLayout.semanticName.c_str(), inputLayout.semanticIndex, DX12ConverterUtils::ConvertToDXGIFormat(inputLayout.format),
            0, inputLayout.alignedByteOffset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});  
    }
    
    // fill out an input layout description structure
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

    // we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
    inputLayoutDesc.NumElements = dxInputLayouts.size();
    inputLayoutDesc.pInputElementDescs = dxInputLayouts.data();

    // compile shader and set bytecode to pipeline state desc
    THROW_IF_FAILED(CompileBindShaders())
    auto& bindVS = dynamic_cast<DX12Shader&>(GetBindShader(RHIShaderType::Vertex));
    D3D12_SHADER_BYTECODE vertexShaderBytecode;
    vertexShaderBytecode.BytecodeLength = bindVS.GetShaderByteCode().size();
    vertexShaderBytecode.pShaderBytecode = bindVS.GetShaderByteCode().data();

    D3D12_SHADER_BYTECODE pixelShaderBytecode;
    if (m_depthStencilState != IRHIDepthStencilMode::DEPTH_WRITE)
    {
        auto& bindPS = dynamic_cast<DX12Shader&>(GetBindShader(RHIShaderType::Pixel));
        pixelShaderBytecode.BytecodeLength = bindPS.GetShaderByteCode().size();
        pixelShaderBytecode.pShaderBytecode = bindPS.GetShaderByteCode().data();    
    }
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
    psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
    psoDesc.pRootSignature = dxRootSignature; // the root signature that describes the input data this pso needs
    psoDesc.VS = vertexShaderBytecode; // structure describing where to find the vertex shader bytecode and how large it is
    if (m_depthStencilState != IRHIDepthStencilMode::DEPTH_WRITE)
    {
        psoDesc.PS = pixelShaderBytecode; // same as VS but for pixel shader    
    }
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing

    {
        unsigned index = 0;
        for (const auto& format : m_bindRenderTargetFormats)
        {
            psoDesc.RTVFormats[index++] = format;    
        }
    }
    
    psoDesc.DSVFormat = m_bindDepthStencilFormat;
    
    psoDesc.SampleDesc = dxSwapchain.GetSwapChainSampleDesc(); // must be the same sample description as the swapchain and depth/stencil buffer
    psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
    switch (m_cullMode) {
        case IRHICullMode::NONE:
            {
                psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;    
            }
            break;
        
        case IRHICullMode::CW:
            {
                psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;     
            }
            break;
        
        case IRHICullMode::CCW:
            {
                psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT;     
            }
        
            break;
        }
    
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); 
    switch (m_depthStencilState) {
    case IRHIDepthStencilMode::DEPTH_READ:
        {
            psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_EQUAL;
            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ZERO;
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
    psoDesc.NumRenderTargets = m_bindRenderTargetFormats.size();

    THROW_IF_FAILED(dxDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateObject)))
    
    return true;
}

IRHIShader& DX12GraphicsPipelineStateObject::GetBindShader(RHIShaderType type)
{
    assert(m_shaders.find(type) != m_shaders.end());
    return *m_shaders[type];
}

bool DX12GraphicsPipelineStateObject::CompileBindShaders()
{
    RETURN_IF_FALSE(!m_shaders.empty())

    for (const auto& shader : m_shaders)
    {
        shader.second->SetShaderCompilePreDefineMacros(m_shaderMacros);
        RETURN_IF_FALSE(shader.second->CompileShader())
    }

    return true;
}

DX12ComputePipelineStateObject::DX12ComputePipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::Compute)
    , m_computePipelineStateDesc({})
{
}
