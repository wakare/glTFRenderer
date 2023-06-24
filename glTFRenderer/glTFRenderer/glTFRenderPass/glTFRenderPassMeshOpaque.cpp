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
    
    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), descriptorHeap, 0,
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
    
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::InitInterface(resourceManager))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneMesh::InitInterface(resourceManager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))
    RETURN_IF_FALSE(resourceManager.GetCurrentFrameFence().SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))

    LOG_FORMAT("[DEBUG] Init MeshPassOpaque finished!\n")
    return true;
}

bool glTFRenderPassMeshOpaque::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::RenderPass(resourceManager))
    
    return true;
}

bool glTFRenderPassMeshOpaque::ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material)
{
    RETURN_IF_FALSE(material.GetMaterialType() == MaterialType::Opaque)
    
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
    return 1;
}

bool glTFRenderPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    // Init root signature
    constexpr size_t rootSignatureParameterCount = MeshOpaquePass_RootParameter_Num;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    RETURN_IF_FALSE(m_rootSignature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount))
    
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupRootSignature(resourceManager))
    
    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 1};
    m_rootSignature->GetRootParameter(MeshOpaquePass_RootParameter_MeshMaterialTexSRV).InitAsDescriptorTableRange(1, &SRVRangeDesc);
    
    m_rootSignature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);
    RETURN_IF_FALSE(m_rootSignature->InitRootSignature(resourceManager.GetDevice()))

    return true;
}

bool glTFRenderPassMeshOpaque::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupPipelineStateObject(resourceManager))
    
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");

    RETURN_IF_FALSE(m_pipelineStateObject->BindInputLayout(GetVertexInputLayout()))
    
    RETURN_IF_FALSE (m_pipelineStateObject->InitPipelineStateObject(resourceManager.GetDevice(), *m_rootSignature, resourceManager.GetSwapchain()))

    return true;
}

bool glTFRenderPassMeshOpaque::BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID)
{
    // Using texture SRV slot when mesh material is texture
    glTFUniqueID matID = m_meshes[meshID].materialID;
    if (matID == glTFUniqueIDInvalid)
    {
        return true;
    }
    
    if (m_materialTextures.find(matID) != m_materialTextures.end())
    {
        RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resourceManager.GetCommandList(),
            MeshOpaquePass_RootParameter_MeshMaterialTexSRV, m_materialTextures[matID]->GetTextureSRVHandle());
        return true;
    }

    return false;
}

std::vector<RHIPipelineInputLayout> glTFRenderPassMeshOpaque::GetVertexInputLayout()
{
    std::vector<RHIPipelineInputLayout> inputLayouts;
    inputLayouts.push_back({g_inputLayoutNamePOSITION, 0, RHIDataFormat::R32G32B32_FLOAT, 0});
    inputLayouts.push_back({g_inputLayoutNameNORMAL, 0, RHIDataFormat::R32G32B32_FLOAT, 12});
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
                inputLayouts.push_back({g_inputLayoutNamePOSITION, 0, RHIDataFormat::R32G32B32_FLOAT, vertexLayoutOffset});
            }
            break;
        case VertexLayoutType::NORMAL:
            {
                inputLayouts.push_back({g_inputLayoutNameNORMAL, 0, RHIDataFormat::R32G32B32_FLOAT, vertexLayoutOffset});
            }
            break;
        case VertexLayoutType::UV:
            {
                inputLayouts.push_back({g_inputLayoutNameTEXCOORD, 0, RHIDataFormat::R32G32_FLOAT, vertexLayoutOffset});
            }
            break;
        }

        vertexLayoutOffset += vertexLayout.byteSize;   
    }

    return inputLayouts;
}
