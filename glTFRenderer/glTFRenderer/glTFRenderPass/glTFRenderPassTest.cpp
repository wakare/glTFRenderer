#include "glTFRenderPassTest.h"

#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"
#include "../glTFRHI/RHIInterface/IRHIIndexBufferView.h"
#include "../glTFUtils/glTFLog.h"

glTFRenderPassTest::glTFRenderPassTest()
    : glTFRenderPassBase()
    , m_rootSignatureParameterCount(2)
    , m_rootSignatureStaticSamplerCount(1)
    , m_cbvGPUHandle(0)
    , m_cbPerObject()
    , m_textureSRVGPUHandle(0)
{
}

glTFRenderPassTest::~glTFRenderPassTest()
= default;

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
    
    const RHIRootParameterDescriptorRangeDesc CBVRangeDesc {RHIRootParameterDescriptorRangeType::CBV, 0, 1};
    m_rootSignature->GetRootParameter(0).InitAsDescriptorTableRange(1, &CBVRangeDesc);

    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 1};
    m_rootSignature->GetRootParameter(1).InitAsDescriptorTableRange(1, &SRVRangeDesc);
    
    m_rootSignature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);
    
    m_rootSignature->InitRootSignature(resourceManager.GetDevice());
    
    m_pipelineStateObject = RHIResourceFactory::CreateRHIResource<IRHIPipelineStateObject>();

    std::vector<IRHIRenderTarget*> allRts;
    allRts.push_back(&resourceManager.GetCurrentFrameSwapchainRT());
    allRts.push_back(m_depthBuffer.get());
    
    m_pipelineStateObject->BindRenderTargets(allRts);
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\vertexShader.hlsl)", RHIShaderType::Vertex);
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\pixelShader.hlsl)", RHIShaderType::Pixel);

    // Init scene objects
    m_box = std::make_shared<glTFSceneBox>();
    
    std::vector<RHIPipelineInputLayout> inputLayouts;
    const auto& boxVertexLayout = m_box->GetVertexLayout();

    unsigned vertexLayoutOffset = 0;
    for (const auto& vertexLayout : boxVertexLayout.elements)
    {
        switch (vertexLayout.type)
        {
            case VertexLayoutType::POSITION:
                {
                    inputLayouts.push_back({"POSITION", 0, RHIDataFormat::R32G32B32_FLOAT, vertexLayoutOffset});
                }
            break;
            case VertexLayoutType::NORMAL:
                {
                    inputLayouts.push_back({"NORMAL", 0, RHIDataFormat::R32G32B32_FLOAT, vertexLayoutOffset});
                }
                break;
            case VertexLayoutType::UV:
                {
                    inputLayouts.push_back({"TEXCOORD", 0, RHIDataFormat::R32G32_FLOAT, vertexLayoutOffset});
                }
                break;
        }

        vertexLayoutOffset += vertexLayout.byteSize;   
    }

    m_camera = std::make_shared<glTFCamera>(45.0f, 800.0f, 600.0f, 0.1f, 1000.0f);
    m_camera->GetTransform() = glTFTransform::Identity();
    m_camera->GetTransform().position = {0.0f, 0.0f, -3.0f};

    if (!m_pipelineStateObject->InitPipelineStateObject(resourceManager.GetDevice(), *m_rootSignature, resourceManager.GetSwapchain(), inputLayouts))
    {
        return false;
    }
    
    const auto& boxVertices = m_box->GetVertexBufferData();
    const auto& boxIndices = m_box->GetIndexBufferData();
    
    // Upload vertex and index buffer
    m_vertexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_indexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

    RHIBufferDesc vertexBufferDesc = {L"vertexBufferDefaultBuffer", boxVertices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    RHIBufferDesc indexBufferDesc = {L"indexBufferDefaultBuffer", boxIndices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    
    RETURN_IF_FALSE(m_vertexBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexBufferDesc ))
    RETURN_IF_FALSE(m_indexBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexBufferDesc ))
    
    m_vertexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_indexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

    RHIBufferDesc vertexUploadBufferDesc = {L"vertexBufferDefaultBuffer", boxVertices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    RHIBufferDesc indexUploadBufferDesc = {L"indexBufferDefaultBuffer", boxIndices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};

    RETURN_IF_FALSE(m_vertexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexUploadBufferDesc ))
    RETURN_IF_FALSE(m_indexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexUploadBufferDesc ))
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *m_vertexUploadBuffer, *m_vertexBuffer, boxVertices.data.get(), boxVertices.byteSize))
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *m_indexUploadBuffer, *m_indexBuffer, boxIndices.data.get(), boxIndices.byteSize))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *m_vertexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER))
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *m_indexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))
    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))

    m_fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    RETURN_IF_FALSE(m_fence->InitFence(resourceManager.GetDevice()))

    RETURN_IF_FALSE(m_fence->SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))
    RETURN_IF_FALSE(m_fence->WaitUtilSignal())

    m_vertexBufferView = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
    m_indexBufferView = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();

    m_vertexBufferView->InitVertexBufferView(*m_vertexBuffer, 0, m_box->GetVertexLayout().GetVertexStride(), boxVertices.byteSize);
    m_indexBufferView->InitIndexBufferView(*m_indexBuffer, 0, RHIDataFormat::R32_UINT, boxIndices.byteSize);

    m_cbvGPUBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_cbvGPUBuffer->InitGPUBuffer(resourceManager.GetDevice(), {L"RenderPass_Test_CBVBuffer", static_cast<size_t>(64 * 1024), 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer }))
    
    m_mainDescriptorHeap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_mainDescriptorHeap->InitDescriptorHeap(resourceManager.GetDevice(), {2,  RHIDescriptorHeapType::CBV_SRV_UAV, true});

    //RETURN_IF_FALSE(RHIUtils::Instance().CreateConstantBufferViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 0, *m_cbvGPUBuffer, {0, sizeof(m_colorMultiplier)}, m_cbvGPUHandle))
    RETURN_IF_FALSE(RHIUtils::Instance().CreateConstantBufferViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 0, *m_cbvGPUBuffer, {0, sizeof(m_cbPerObject)}, m_cbvGPUHandle))
    
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    glTFImageLoader imageLoader;
    imageLoader.InitImageLoader();
    
    m_textureBuffer = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    RETURN_IF_FALSE(m_textureBuffer->UploadTextureFromFile(resourceManager.GetDevice(), resourceManager.GetCommandList(), imageLoader, L"glTFResources/tiger.bmp"))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 1,
                                                                  m_textureBuffer->GetGPUBuffer(), {m_textureBuffer->GetTextureDesc().GetDataFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_textureSRVGPUHandle))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))
    RETURN_IF_FALSE(resourceManager.GetCurrentFrameFence().SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))
    
    LOG_FORMAT_FLUSH("[RenderPass_Test] Init Pass resource finished!\n")
    
    return true;
}

bool glTFRenderPassTest::RenderPass(glTFRenderResourceManager& resourceManager)
{
    //UpdateColorMultiplier();
    UpdateConstantBufferPerObject();
    
    RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator(), *m_pipelineStateObject);

    RHIUtils::Instance().SetRootSignature(resourceManager.GetCommandList(), *m_rootSignature);

    RHIUtils::Instance().SetDescriptorHeap(resourceManager.GetCommandList(), m_mainDescriptorHeap.get(), 1);
    RHIUtils::Instance().SetGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), 0, m_cbvGPUHandle);
    RHIUtils::Instance().SetGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), 1, m_textureSRVGPUHandle);
    
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
    RHIUtils::Instance().DrawIndexInstanced(resourceManager.GetCommandList(), m_box->GetIndexBufferData().IndexCount(), m_box->GetInstanceCount(), 0, 0, 0);

    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameSwapchainRT(),
            RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PRESENT);
    
    return true;
}

void glTFRenderPassTest::UpdateConstantBufferPerObject()
{
    // rotate box per tick
    m_box->GetTransform().rotation.x += 0.0001f;
    m_box->GetTransform().rotation.y += 0.0002f;
    m_box->GetTransform().rotation.z += 0.0003f;
    
    m_cbPerObject.mvpMat = m_camera->GetViewProjectionMatrix() * m_box->GetTransformMatrix();
    m_cbvGPUBuffer->UploadBufferFromCPU(&m_cbPerObject, sizeof(m_cbPerObject));
}
