#include "glTFRenderPassMeshOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"

bool glTFRenderPassMeshOpaque::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::InitPass(resourceManager))
    
    // Setup root signature and pipeline state object
    assert(m_rootSignature && m_pipelineStateObject);

    // Init root signature
    constexpr size_t rootSignatureParameterCount = 2;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    m_rootSignature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount);
    
    const RHIRootParameterDescriptorRangeDesc CBVRangeDesc {RHIRootParameterDescriptorRangeType::CBV, 0, 1};
    m_rootSignature->GetRootParameter(0).InitAsDescriptorTableRange(1, &CBVRangeDesc);

    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 1};
    m_rootSignature->GetRootParameter(1).InitAsDescriptorTableRange(1, &SRVRangeDesc);
    
    m_rootSignature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);
    m_rootSignature->InitRootSignature(resourceManager.GetDevice());

    // Init pipeline state object
    std::vector<IRHIRenderTarget*> allRts;
    allRts.push_back(&resourceManager.GetCurrentFrameSwapchainRT());
    allRts.push_back(m_depthBuffer.get());
    
    m_pipelineStateObject->BindRenderTargets(allRts);
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");

    RETURN_IF_FALSE (m_pipelineStateObject->InitPipelineStateObject(resourceManager.GetDevice(), *m_rootSignature, resourceManager.GetSwapchain(), GetVertexInputLayout()))
    
    // Load image as texture SRV
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    glTFImageLoader imageLoader;
    imageLoader.InitImageLoader();
    
    m_textureBuffer = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    RETURN_IF_FALSE(m_textureBuffer->UploadTextureFromFile(resourceManager.GetDevice(), resourceManager.GetCommandList(), imageLoader, L"glTFResources/tiger.bmp"))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 1,
                                                                  m_textureBuffer->GetGPUBuffer(), {m_textureBuffer->GetTextureDesc().GetDataFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_textureSRVHandle))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))
    RETURN_IF_FALSE(resourceManager.GetCurrentFrameFence().SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))

    LOG_FORMAT("[DEBUG] Init MeshPassOpaque finished!")
    return true;
}

bool glTFRenderPassMeshOpaque::RenderPass(glTFRenderResourceManager& resourceManager)
{
    if (!glTFRenderPassMeshBase::RenderPass(resourceManager))
    {
        return false;
    }

    RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator(), *m_pipelineStateObject);

    RHIUtils::Instance().SetRootSignature(resourceManager.GetCommandList(), *m_rootSignature);

    RHIUtils::Instance().SetDescriptorHeap(resourceManager.GetCommandList(), m_mainDescriptorHeap.get(), 1);
    RHIUtils::Instance().SetGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), 0, m_perMeshCBHandle);
    RHIUtils::Instance().SetGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), 1, m_textureSRVHandle);
    
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

    for (const auto& mesh : m_meshes)
    {
        // Upload constant buffer
        m_constantBufferPerObject.worldMat = mesh.second.meshTransformMatrix;
        m_perMeshConstantBuffer->UploadBufferFromCPU(&m_constantBufferPerObject, sizeof(m_constantBufferPerObject));
        
        RHIUtils::Instance().SetVertexBufferView(resourceManager.GetCommandList(), *mesh.second.meshVertexBufferView);
        RHIUtils::Instance().SetIndexBufferView(resourceManager.GetCommandList(), *mesh.second.meshIndexBufferView);

        RHIUtils::Instance().SetPrimitiveTopology( resourceManager.GetCommandList(), RHIPrimitiveTopologyType::TRIANGLELIST);
        RHIUtils::Instance().DrawIndexInstanced(resourceManager.GetCommandList(), mesh.second.meshIndexCount, 1, 0, 0, 0);    
    }

    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameSwapchainRT(),
            RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PRESENT);
    
    return true;
}

size_t glTFRenderPassMeshOpaque::GetMainDescriptorHeapSize()
{
    return 2;
}

std::vector<RHIPipelineInputLayout> glTFRenderPassMeshOpaque::GetVertexInputLayout()
{
    std::vector<RHIPipelineInputLayout> inputLayouts;
    inputLayouts.push_back({"POSITION", 0, RHIDataFormat::R32G32B32_FLOAT, 0});
    inputLayouts.push_back({"TEXCOORD", 0, RHIDataFormat::R32G32_FLOAT, 12});
    return inputLayouts;
}

std::vector<RHIPipelineInputLayout> glTFRenderPassMeshOpaque::ResolveVertexInputLayout(
    const glTFScenePrimitive& primitive)
{
    std::vector<RHIPipelineInputLayout> inputLayouts;
    const auto& boxVertexLayout = primitive.GetVertexLayout();

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

    return inputLayouts;
}
