#include "glTFRenderPassPostprocess.h"

#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassPostprocess::glTFRenderPassPostprocess()
{
}

bool glTFRenderPassPostprocess::InitPass(glTFRenderResourceManager& resourceManager)
{
    float postprocessVertices[] =
    {
        // position + uv
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
    };

    unsigned postprocessIndices[] =
    {
        0, 3, 1,
        0, 2, 3, 
    };
    
    m_postprocessQuadResource.meshVertexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_postprocessQuadResource.meshIndexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

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
    
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *vertexUploadBuffer, *m_postprocessQuadResource.meshVertexBuffer, postprocessVertices, sizeof(postprocessVertices)))
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *indexUploadBuffer, *m_postprocessQuadResource.meshIndexBuffer, postprocessIndices, sizeof(postprocessIndices)))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *m_postprocessQuadResource.meshVertexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER))
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *m_postprocessQuadResource.meshIndexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))
    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))
    
    return true;
}

void glTFRenderPassPostprocess::DrawPostprocessQuad(glTFRenderResourceManager& resourceManager)
{
    
}
