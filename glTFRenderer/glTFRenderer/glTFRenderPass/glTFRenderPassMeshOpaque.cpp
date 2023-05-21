#include "glTFRenderPassMeshOpaque.h"

#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"

bool glTFAlbedoMaterialTextureResource::Init(glTFRenderResourceManager& resourceManager, IRHIDescriptorHeap& descriptorHeap)
{
    glTFImageLoader imageLoader;
    imageLoader.InitImageLoader();

    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    
    m_textureBuffer = RHIResourceFactory::CreateRHIResource<IRHITexture>();
    RETURN_IF_FALSE(m_textureBuffer->UploadTextureFromFile(resourceManager.GetDevice(), resourceManager.GetCommandList(),
        imageLoader, m_sourceTexture.GetTexturePath()))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), descriptorHeap, 1,
        m_textureBuffer->GetGPUBuffer(), {m_textureBuffer->GetTextureDesc().GetDataFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_textureSRVHandle))

    RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()); 
    RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(), resourceManager.GetCommandQueue());
    
    return true;
}

RHICPUDescriptorHandle glTFAlbedoMaterialTextureResource::GetTextureSRVHandle() const
{
    return m_textureSRVHandle;
}

bool glTFRenderPassMeshOpaque::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::InitPass(resourceManager))
    
    
    // Load image as texture SRV
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    glTFImageLoader imageLoader;
    imageLoader.InitImageLoader();
    
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

    resourceManager.GetRenderTargetManager().BindRenderTarget(resourceManager.GetCommandList(),
        &resourceManager.GetCurrentFrameSwapchainRT(), 1, m_depthBuffer.get());

    resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(), &resourceManager.GetCurrentFrameSwapchainRT(), 1);
    resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(),m_depthBuffer.get(), 1);
    
    const RHIViewportDesc viewport = {0, 0, (float)resourceManager.GetSwapchain().GetWidth(), (float)resourceManager.GetSwapchain().GetHeight(), 0.0f, 1.0f };
    RHIUtils::Instance().SetViewport(resourceManager.GetCommandList(), viewport);

    const RHIScissorRectDesc scissorRect = {0, 0, resourceManager.GetSwapchain().GetWidth(), resourceManager.GetSwapchain().GetHeight() }; 
    RHIUtils::Instance().SetScissorRect(resourceManager.GetCommandList(), scissorRect);

    unsigned meshIndex = 0;
    const size_t offsetStripe = (sizeof(ConstantBufferPerMesh) + 255) & ~255;
    for (const auto& mesh : m_meshes)
    {
        const size_t dataOffset = meshIndex++ * offsetStripe;
        
        // Upload constant buffer
        m_constantBufferPerObject.worldMat = mesh.second.meshTransformMatrix;
        m_perMeshConstantBuffer->UploadBufferFromCPU(&m_constantBufferPerObject, dataOffset, sizeof(m_constantBufferPerObject));

        RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), 0, m_perMeshConstantBuffer->GetGPUBufferHandle() + dataOffset);

        // Using texture SRV slot when mesh material is texture
        if (m_materialTextures.find(mesh.second.materialID) != m_materialTextures.end())
        {
            RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), 1, m_materialTextures[mesh.second.materialID]->GetTextureSRVHandle());    
        }
        
        RHIUtils::Instance().SetVertexBufferView(resourceManager.GetCommandList(), *mesh.second.meshVertexBufferView);
        RHIUtils::Instance().SetIndexBufferView(resourceManager.GetCommandList(), *mesh.second.meshIndexBufferView);

        RHIUtils::Instance().SetPrimitiveTopology( resourceManager.GetCommandList(), RHIPrimitiveTopologyType::TRIANGLELIST);
        RHIUtils::Instance().DrawIndexInstanced(resourceManager.GetCommandList(), mesh.second.meshIndexCount, 1, 0, 0, 0);    
    }
    
    return true;
}

bool glTFRenderPassMeshOpaque::ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material)
{
    if (material.GetMaterialType() != MaterialType::Opaque)
    {
        return false;
    }

    const auto& OpaqueMaterial = dynamic_cast<const glTFMaterialOpaque&>(material);

    if (m_materialTextures.end() == m_materialTextures.find(OpaqueMaterial.GetID()))
    {
        if (OpaqueMaterial.UsingAlbedoTexture())
        {
            m_materialTextures[material.GetID()] = std::make_unique<glTFAlbedoMaterialTextureResource>(OpaqueMaterial.GetAlbedoTexture());
            const bool init = m_materialTextures[material.GetID()]->Init(resourceManager, *m_mainDescriptorHeap);
            assert(init);
        }
    }
    
    return true;
}

size_t glTFRenderPassMeshOpaque::GetMainDescriptorHeapSize()
{
    return 2;
}

bool glTFRenderPassMeshOpaque::SetupMainDescriptorHeap(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(m_mainDescriptorHeap->InitDescriptorHeap(resourceManager.GetDevice(), {static_cast<unsigned>(GetMainDescriptorHeapSize()),  RHIDescriptorHeapType::CBV_SRV_UAV, true}))
    return true;
}

bool glTFRenderPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    // Init root signature
    constexpr size_t rootSignatureParameterCount = 2;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    RETURN_IF_FALSE(m_rootSignature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount))
    
    //const RHIRootParameterDescriptorRangeDesc CBVRangeDesc {RHIRootParameterDescriptorRangeType::CBV, 0, 1};
    m_rootSignature->GetRootParameter(0).InitAsCBV(0);

    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 1};
    m_rootSignature->GetRootParameter(1).InitAsDescriptorTableRange(1, &SRVRangeDesc);
    
    m_rootSignature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);
    RETURN_IF_FALSE(m_rootSignature->InitRootSignature(resourceManager.GetDevice()))

    return true;
}

bool glTFRenderPassMeshOpaque::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    // Init pipeline state object
    
    const auto width = resourceManager.GetSwapchain().GetWidth();
    const auto height = resourceManager.GetSwapchain().GetHeight();
    auto& m_renderTargetManager = resourceManager.GetRenderTargetManager();

    RHIRenderTargetClearValue clearValue{};
    clearValue.clearDS.clearDepth = 1.0f;
    clearValue.clearDS.clearStencilValue = 0;
    
    m_depthBuffer = m_renderTargetManager.CreateRenderTarget(resourceManager.GetDevice(), RHIRenderTargetType::DSV, RHIDataFormat::D32_FLOAT,
        IRHIRenderTargetDesc{width, height, false, clearValue, "MeshPassBase_DepthRT"});
    
    std::vector<IRHIRenderTarget*> allRts;
    allRts.push_back(&resourceManager.GetCurrentFrameSwapchainRT());
    allRts.push_back(m_depthBuffer.get());
    
    m_pipelineStateObject->BindRenderTargets(allRts);
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");

    RETURN_IF_FALSE (m_pipelineStateObject->InitPipelineStateObject(resourceManager.GetDevice(), *m_rootSignature, resourceManager.GetSwapchain(), GetVertexInputLayout()))

    return true;
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
