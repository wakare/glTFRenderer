#include "glTFRenderPassPostprocess.h"

#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassPostprocess::glTFRenderPassPostprocess()
{
}

bool glTFRenderPassPostprocess::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::InitPass(resourceManager))
    
    float postprocessVertices[] =
    {
        // position + uv
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 
        -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
    };

    unsigned postprocessIndices[] =
    {
        0, 1, 3,
        0, 3, 2, 
    };
    
    m_postprocessQuadResource.meshVertexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_postprocessQuadResource.meshIndexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    auto& vertexBufferView = m_postprocessQuadResource.meshVertexBufferView = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
    auto& indexBufferView = m_postprocessQuadResource.meshIndexBufferView = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
    
    const RHIBufferDesc vertexBufferDesc = {L"vertexBufferDefaultBuffer", sizeof(postprocessVertices), 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    const RHIBufferDesc indexBufferDesc = {L"indexBufferDefaultBuffer", sizeof(postprocessIndices), 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};

    RETURN_IF_FALSE(m_postprocessQuadResource.meshVertexBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexBufferDesc ))
    RETURN_IF_FALSE(m_postprocessQuadResource.meshIndexBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexBufferDesc ))

    const auto vertexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    const auto indexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    
    const RHIBufferDesc vertexUploadBufferDesc = {L"vertexBufferUploadBuffer", sizeof(postprocessVertices), 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    const RHIBufferDesc indexUploadBufferDesc = {L"indexBufferUploadBuffer", sizeof(postprocessIndices), 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};

    RETURN_IF_FALSE(vertexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexUploadBufferDesc ))
    RETURN_IF_FALSE(indexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexUploadBufferDesc ))
    
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandListForRecord(), *vertexUploadBuffer, *m_postprocessQuadResource.meshVertexBuffer, postprocessVertices, sizeof(postprocessVertices)))
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandListForRecord(), *indexUploadBuffer, *m_postprocessQuadResource.meshIndexBuffer, postprocessIndices, sizeof(postprocessIndices)))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandListForRecord(), *m_postprocessQuadResource.meshVertexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER))
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandListForRecord(), *m_postprocessQuadResource.meshIndexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))
    
    vertexBufferView->InitVertexBufferView(*m_postprocessQuadResource.meshVertexBuffer, 0, 20, sizeof(postprocessVertices));
    indexBufferView->InitIndexBufferView(*m_postprocessQuadResource.meshIndexBuffer, 0, RHIDataFormat::R32_UINT, sizeof(postprocessIndices));

    auto fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    RETURN_IF_FALSE(fence->InitFence(resourceManager.GetDevice()))

    resourceManager.CloseCommandListAndExecute(true);
    
    return true;
}

bool glTFRenderPassPostprocess::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    return true;
}

bool glTFRenderPassPostprocess::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupPipelineStateObject(resourceManager));
    
    return true;
}

void glTFRenderPassPostprocess::DrawPostprocessQuad(glTFRenderResourceManager& resourceManager)
{
    RHIUtils::Instance().SetVertexBufferView(resourceManager.GetCommandListForRecord(), *m_postprocessQuadResource.meshVertexBufferView);
    RHIUtils::Instance().SetIndexBufferView(resourceManager.GetCommandListForRecord(), *m_postprocessQuadResource.meshIndexBufferView);

    RHIUtils::Instance().SetPrimitiveTopology( resourceManager.GetCommandListForRecord(), RHIPrimitiveTopologyType::TRIANGLELIST);
    RHIUtils::Instance().DrawIndexInstanced(resourceManager.GetCommandListForRecord(), 6, 1, 0, 0, 0);    
}

std::vector<RHIPipelineInputLayout> glTFRenderPassPostprocess::GetVertexInputLayout()
{
    std::vector<RHIPipelineInputLayout> inputLayouts;
    inputLayouts.push_back({"POSITION", 0, RHIDataFormat::R32G32B32_FLOAT, 0});
    inputLayouts.push_back({"TEXCOORD", 0, RHIDataFormat::R32G32_FLOAT, 12});
    return inputLayouts;
}
