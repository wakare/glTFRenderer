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

    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(glTFRenderResourceManager::GetDevice(), vertexBufferDesc, m_postprocessQuadResource.meshVertexBuffer);
    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(glTFRenderResourceManager::GetDevice(), indexBufferDesc, m_postprocessQuadResource.meshIndexBuffer);
    
    auto vertexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIBufferAllocation>();
    auto indexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIBufferAllocation>();
    
    const RHIBufferDesc vertexUploadBufferDesc = {L"vertexBufferUploadBuffer", sizeof(postprocessVertices), 1, 1, RHIBufferType::Upload, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};
    const RHIBufferDesc indexUploadBufferDesc = {L"indexBufferUploadBuffer", sizeof(postprocessIndices), 1, 1, RHIBufferType::Upload, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};

    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(glTFRenderResourceManager::GetDevice(), vertexUploadBufferDesc, vertexUploadBuffer);
    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(glTFRenderResourceManager::GetDevice(), indexUploadBufferDesc, indexUploadBuffer);
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *vertexUploadBuffer->m_buffer, *m_postprocessQuadResource.meshVertexBuffer->m_buffer, postprocessVertices, sizeof(postprocessVertices)))
    RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *indexUploadBuffer->m_buffer, *m_postprocessQuadResource.meshIndexBuffer->m_buffer, postprocessIndices, sizeof(postprocessIndices)))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *m_postprocessQuadResource.meshVertexBuffer->m_buffer, RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER))
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *m_postprocessQuadResource.meshIndexBuffer->m_buffer, RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_INDEX_BUFFER))

    vertexBufferView->InitVertexBufferView(*m_postprocessQuadResource.meshVertexBuffer->m_buffer, 0, 20, sizeof(postprocessVertices));
    indexBufferView->InitIndexBufferView(*m_postprocessQuadResource.meshIndexBuffer->m_buffer, 0, RHIDataFormat::R32_UINT, sizeof(postprocessIndices));

    auto fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    RETURN_IF_FALSE(fence->InitFence(resource_manager.GetDevice()))

    resource_manager.CloseCommandListAndExecute(true);
    
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

const std::vector<RHIPipelineInputLayout>& glTFGraphicsPassPostprocess::GetVertexInputLayout(
    glTFRenderResourceManager& resource_manager)
{
    std::vector<RHIPipelineInputLayout> inputLayouts;
    inputLayouts.push_back({"POSITION", 0, RHIDataFormat::R32G32B32_FLOAT, 0});
    inputLayouts.push_back({"TEXCOORD", 0, RHIDataFormat::R32G32_FLOAT, 12});
    return inputLayouts;
}
