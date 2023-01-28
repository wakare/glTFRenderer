#include "glTFRenderPassTest.h"

#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassTest::glTFRenderPassTest()
    : m_rootSignatureParameterCount(1)
    , m_rootSignatureStaticSamplerCount(0)
{
}

const char* glTFRenderPassTest::PassName()
{
    return "RenderPass_Test";
}

bool glTFRenderPassTest::InitPass(glTFRenderResourceManager& resourceManager)
{
    const auto width = resourceManager.GetSwapchain().GetWidth();
    const auto height = resourceManager.GetSwapchain().GetHeight();
    auto& m_renderTargetManager = resourceManager.GetRenderTargetManager();
    
    RHIRenderTargetClearValue clearValue{};
    clearValue.clearDS.clearDepth = 1.0f;
    clearValue.clearDS.clearStencilValue = 0;
    
    m_depthBuffer = m_renderTargetManager.CreateRenderTarget(resourceManager.GetDevice(), RHIRenderTargetType::DSV, RHIDataFormat::D32_FLOAT,
        IRHIRenderTargetDesc{width, height, false, clearValue, "RenderPass_Test_Depth"});

    m_rootSignature = RHIResourceFactory::CreateRHIResource<IRHIRootSignature>();
    m_rootSignature->AllocateRootSignatureSpace(m_rootSignatureParameterCount, m_rootSignatureStaticSamplerCount);
    auto& rootParameter = m_rootSignature->GetRootParameter(0);
    
    const RHIRootParameterDescriptorRangeDesc rangeDesc {RHIRootParameterDescriptorRangeType::CBV, 0, 1};
    rootParameter.InitAsDescriptorTableRange(1, &rangeDesc);
    
    m_rootSignature->InitRootSignature(resourceManager.GetDevice());
    
    m_pipelineStateObject = RHIResourceFactory::CreateRHIResource<IRHIPipelineStateObject>();

    std::vector<IRHIRenderTarget*> allRts;
    allRts.push_back(&resourceManager.GetCurrentFrameSwapchainRT());
    allRts.push_back(m_depthBuffer.get());
    
    m_pipelineStateObject->BindRenderTargets(allRts);
    m_pipelineStateObject->BindShaderCode(
        R"(D:\Work\DevSpace\glTFRenderer\glTFRenderer\glTFRenderer\glTFRenderer\glTFShaders\vertexShader.hlsl)", RHIShaderType::Vertex);
    m_pipelineStateObject->BindShaderCode(
        R"(D:\Work\DevSpace\glTFRenderer\glTFRenderer\glTFRenderer\glTFRenderer\glTFShaders\pixelShader.hlsl)", RHIShaderType::Pixel);

    std::vector<RHIPipelineInputLayout> inputLayouts;
    inputLayouts.push_back({"POSITION", 0, RHIDataFormat::R32G32B32_FLOAT, 0});
    inputLayouts.push_back({"COLOR", 0, RHIDataFormat::R32G32B32A32_FLOAT, 12});
    
    if (!m_pipelineStateObject->InitPipelineStateObject(resourceManager.GetDevice(), *m_rootSignature, resourceManager.GetSwapchain(), inputLayouts))
    {
        return false;
    }
    
    return true;
}

bool glTFRenderPassTest::RenderPass(glTFRenderResourceManager& resourceManager)
{
    return true;
}
