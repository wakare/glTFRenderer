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

bool DX12GraphicsPipelineStateObject::BindShaderCode(const std::string& shaderFilePath, RHIShaderType type)
{
    std::shared_ptr<DX12Shader> dxShader = RHIResourceFactory::CreateRHIResource<DX12Shader>();
    if (!dxShader->InitShader(shaderFilePath, type))
    {
        return false;
    }

    // Delay compile shader bytecode util create pso
    
    return true;
}

bool DX12GraphicsPipelineStateObject::BindRenderTargets(const std::vector<IRHIRenderTarget>& renderTargets)
{
    
    
    return true;
}

bool DX12GraphicsPipelineStateObject::InitPipelineStateObject(IRHIDevice& device, IRHIRootSignature& rootSignature, IRHISwapChain& swapchain, const std::vector<RHIPipelineInputLayout>& inputLayouts)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxRootSignature = dynamic_cast<DX12RootSignature&>(rootSignature).GetRootSignature();
    auto& dxSwapchain = dynamic_cast<DX12SwapChain&>(swapchain);
    
    // create input layout

    // The input layout is used by the Input Assembler so that it knows
    // how to read the vertex data bound to it.

    std::vector<D3D12_INPUT_ELEMENT_DESC> dxInputLayouts;
    for (const auto& inputLayout : inputLayouts)
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
    auto& bindVS = dynamic_cast<DX12Shader&>(GetBindShader(RHIShaderType::Vertex));
    auto& bindPS = dynamic_cast<DX12Shader&>(GetBindShader(RHIShaderType::Pixel));

    D3D12_SHADER_BYTECODE vertexShaderBytecode;
    vertexShaderBytecode.BytecodeLength = bindVS.GetShaderByteCode().size();
    vertexShaderBytecode.pShaderBytecode = bindVS.GetShaderByteCode().data();

    D3D12_SHADER_BYTECODE pixelShaderBytecode;
    pixelShaderBytecode.BytecodeLength = bindPS.GetShaderByteCode().size();
    pixelShaderBytecode.pShaderBytecode = bindPS.GetShaderByteCode().data();
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
    psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
    psoDesc.pRootSignature = dxRootSignature; // the root signature that describes the input data this pso needs
    psoDesc.VS = vertexShaderBytecode; // structure describing where to find the vertex shader bytecode and how large it is
    psoDesc.PS = pixelShaderBytecode; // same as VS but for pixel shader
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
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = m_bindRenderTargetFormats.size(); // we are only binding one render target

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
    if (m_shaders.empty())
    {
        return false;
    }

    for (const auto& shader : m_shaders)
    {
        shader.second->CompileShader();
    }

    return true;
}

DX12ComputePipelineStateObject::DX12ComputePipelineStateObject()
    : IRHIPipelineStateObject(RHIPipelineType::Compute)
    , m_computePipelineStateDesc({})
{
}
