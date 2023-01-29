#include "glTFRenderPassTest.h"

#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/IRHIIndexBufferView.h"
#include "../glTFUtils/glTFLog.h"

glTFRenderPassTest::glTFRenderPassTest()
    : m_rootSignatureParameterCount(1)
    , m_rootSignatureStaticSamplerCount(0)
{
}

const char* glTFRenderPassTest::PassName()
{
    return "RenderPass_Test";
}

struct Vertex
{
    float data[3];
    float color[4];
};

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

    // a triangle
    Vertex vList[] = {
        // first quad (closer to camera, blue)
        { -0.5f,  0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        {  0.5f,  0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
    };
    
    // a quad (2 triangles)
    DWORD iList[] = {
        0, 1, 2, // first triangle
        0, 3, 1 // second triangle
    };

    const size_t vBufferSize = sizeof(vList);
    const size_t iBufferSize = sizeof(iList);
    
    // Upload vertex and index buffer
    m_vertexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_indexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

    RHIBufferDesc vertexBufferDesc = {L"vertexBufferDefaultBuffer", vBufferSize, RHIBufferType::Default};
    RHIBufferDesc indexBufferDesc = {L"indexBufferDefaultBuffer", iBufferSize, RHIBufferType::Default};
    
    if (!m_vertexBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexBufferDesc ) ||
        !m_indexBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexBufferDesc ))
    {
        return false;
    }
    
    m_vertexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_indexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

    RHIBufferDesc vertexUploadBufferDesc = {L"vertexBufferDefaultBuffer", vBufferSize, RHIBufferType::Upload};
    RHIBufferDesc indexUploadBufferDesc = {L"indexBufferDefaultBuffer", iBufferSize, RHIBufferType::Upload};

    if (!m_vertexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexUploadBufferDesc ) ||
        !m_indexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexUploadBufferDesc ))
    {
        return false;
    }

    if (!RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    {
        return false;
    }

    if (!RHIUtils::Instance().UploadDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *m_vertexUploadBuffer, *m_vertexBuffer, vList, sizeof(vList)) ||
        !RHIUtils::Instance().UploadDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *m_indexUploadBuffer, *m_indexBuffer, iList, sizeof(iList)))
    {
        return false;
    }

    if (!RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *m_vertexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER) ||
        !RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *m_indexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))
    {
        return false;
    }

    
    if (!RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()) ||
        !RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))
    {
        return false;
    }

    m_fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    m_fence->InitFence(resourceManager.GetDevice());

    m_fence->SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue());
    m_fence->WaitUtilSignal();

    m_vertexBufferView = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
    m_indexBufferView = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();

    m_vertexBufferView->InitVertexBufferView(*m_vertexBuffer, 0, sizeof(Vertex), sizeof(vList));
    m_indexBufferView->InitIndexBufferView(*m_indexBuffer, 0, RHIDataFormat::R32_UINT, sizeof(iList));
    
    LOG_FORMAT_FLUSH("[RenderPass_Test] Init Pass resource finished!")
    
    return true;
}

bool glTFRenderPassTest::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator(), *m_pipelineStateObject);

    RHIUtils::Instance().SetRootSignature(resourceManager.GetCommandList(), *m_rootSignature);
    
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::PRESENT, RHIResourceStateType::RENDER_TARGET);

    resourceManager.GetRenderTargetManager().BindRenderTarget(resourceManager.GetCommandList(),
        &resourceManager.GetCurrentFrameSwapchainRT(), 1, m_depthBuffer.get());

    resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(), &resourceManager.GetCurrentFrameSwapchainRT(), 1);
    resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(),m_depthBuffer.get(), 1);
    
    const RHIViewportDesc viewport = {0, 0, (float)resourceManager.GetSwapchain().GetWidth(), (float)resourceManager.GetSwapchain().GetHeight(), 0.0f, 1.0f };
    RHIUtils::Instance().SetViewport(resourceManager.GetCommandList(), viewport);

    const RHIScissorRectDesc scissorRect = {0, 0, resourceManager.GetSwapchain().GetWidth(), resourceManager.GetSwapchain().GetHeight() }; 
    RHIUtils::Instance().SetScissorRect(resourceManager.GetCommandList(), scissorRect);
    
    RHIUtils::Instance().SetVertexBufferView(resourceManager.GetCommandList(), *m_vertexBufferView);
    RHIUtils::Instance().SetIndexBufferView(resourceManager.GetCommandList(), *m_indexBufferView);

    RHIUtils::Instance().SetPrimitiveTopology( resourceManager.GetCommandList(), RHIPrimitiveTopologyType::TRIANGLELIST);
    RHIUtils::Instance().DrawIndexInstanced(resourceManager.GetCommandList(), 6, 1, 0, 0, 0);

    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameSwapchainRT(),
            RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PRESENT);
    
    return true;
}
