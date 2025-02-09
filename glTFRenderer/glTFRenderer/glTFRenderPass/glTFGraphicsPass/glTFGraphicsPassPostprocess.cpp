#include "glTFGraphicsPassPostprocess.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassPostprocess::glTFGraphicsPassPostprocess()
= default;

bool glTFGraphicsPassPostprocess::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))
    
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
    
    auto& vertexBufferView = m_postprocessQuadResource.meshVertexBufferView = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
    auto& indexBufferView = m_postprocessQuadResource.meshIndexBufferView = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
    
    const RHIBufferDesc vertexBufferDesc = {L"vertexBufferDefaultBuffer", sizeof(postprocessVertices), 1, 1, RHIBufferType::Default, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};
    const RHIBufferDesc indexBufferDesc = {L"indexBufferDefaultBuffer", sizeof(postprocessIndices), 1, 1, RHIBufferType::Default, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};

    auto& memory_manager = resource_manager.GetMemoryManager();
    
    memory_manager.AllocateBufferMemory(resource_manager.GetDevice(), vertexBufferDesc, m_postprocessQuadResource.meshVertexBuffer);
    memory_manager.AllocateBufferMemory(resource_manager.GetDevice(), indexBufferDesc, m_postprocessQuadResource.meshIndexBuffer);
    
    auto vertexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIBufferAllocation>();
    auto indexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIBufferAllocation>();
    
    const RHIBufferDesc vertexUploadBufferDesc = {L"vertexBufferUploadBuffer", sizeof(postprocessVertices), 1, 1, RHIBufferType::Upload, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};
    const RHIBufferDesc indexUploadBufferDesc = {L"indexBufferUploadBuffer", sizeof(postprocessIndices), 1, 1, RHIBufferType::Upload, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};

    memory_manager.AllocateBufferMemory(resource_manager.GetDevice(), vertexUploadBufferDesc, vertexUploadBuffer);
    memory_manager.AllocateBufferMemory(resource_manager.GetDevice(), indexUploadBufferDesc, indexUploadBuffer);
    
    auto& command_list = resource_manager.GetCommandListForRecord();

    memory_manager.UploadBufferData(*vertexUploadBuffer, postprocessVertices, 0, sizeof(postprocessVertices));
    memory_manager.UploadBufferData(*indexUploadBuffer, postprocessIndices, 0, sizeof(postprocessIndices));

    RHIUtils::Instance().CopyBuffer(command_list, *m_postprocessQuadResource.meshVertexBuffer->m_buffer, 0, *vertexUploadBuffer->m_buffer, 0, sizeof(postprocessVertices));
    RHIUtils::Instance().CopyBuffer(command_list, *m_postprocessQuadResource.meshIndexBuffer->m_buffer, 0, *indexUploadBuffer->m_buffer, 0, sizeof(postprocessIndices));

    m_postprocessQuadResource.meshVertexBuffer->m_buffer->Transition(command_list, RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_postprocessQuadResource.meshIndexBuffer->m_buffer->Transition(command_list, RHIResourceStateType::STATE_INDEX_BUFFER);
    
    vertexBufferView->InitVertexBufferView(*m_postprocessQuadResource.meshVertexBuffer->m_buffer, 0, 20, sizeof(postprocessVertices));
    RHIIndexBufferViewDesc desc{};
    desc.size = sizeof(postprocessIndices);
    desc.offset = 0;
    desc.format = RHIDataFormat::R32_UINT;
    indexBufferView->InitIndexBufferView(*m_postprocessQuadResource.meshIndexBuffer->m_buffer, desc);

    auto fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    RETURN_IF_FALSE(fence->InitFence(resource_manager.GetDevice()))

    return true;
}

bool glTFGraphicsPassPostprocess::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))
    
    return true;
}

bool glTFGraphicsPassPostprocess::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::RenderPass(resource_manager))

    DrawPostprocessQuad(resource_manager);
    
    return true;
}

bool glTFGraphicsPassPostprocess::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PostRenderPass(resource_manager))

    return true;
}

bool glTFGraphicsPassPostprocess::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupRootSignature(resourceManager))
    
    return true;
}

bool glTFGraphicsPassPostprocess::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resourceManager))

    VertexAttributeElement position;
    position.type = VertexAttributeType::VERTEX_POSITION;
    position.byte_size = GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT);
    
    VertexAttributeElement uv;
    uv.type = VertexAttributeType::VERTEX_TEXCOORD0;
    uv.byte_size = GetBytePerPixelByFormat(RHIDataFormat::R32G32_FLOAT);
    
    VertexLayoutDeclaration declaration;
    declaration.elements.push_back(position);
    declaration.elements.push_back(uv);
    
    m_vertex_streaming_manager.Init(declaration);
    
    return true;
}

void glTFGraphicsPassPostprocess::DrawPostprocessQuad(glTFRenderResourceManager& resourceManager)
{
    auto& command_list = resourceManager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetVertexBufferView(command_list, 0, *m_postprocessQuadResource.meshVertexBufferView);
    RHIUtils::Instance().SetIndexBufferView(command_list, *m_postprocessQuadResource.meshIndexBufferView);

    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);
    RHIUtils::Instance().DrawIndexInstanced(command_list, 6, 1, 0, 0, 0);    
}

const RHIVertexStreamingManager& glTFGraphicsPassPostprocess::GetVertexStreamingManager(
    glTFRenderResourceManager& resource_manager) const
{
    return m_vertex_streaming_manager;
}