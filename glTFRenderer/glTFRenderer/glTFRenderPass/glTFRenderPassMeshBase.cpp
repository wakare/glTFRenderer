#include "glTFRenderPassMeshBase.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassMeshBase::glTFRenderPassMeshBase()
    : m_constantBufferPerObject({})
    , m_perMeshCBHandle(0)
{
}

bool glTFRenderPassMeshBase::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE (glTFRenderPassBase::InitPass(resourceManager))

    m_perMeshConstantBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    
    // TODO: Calculate mesh constant buffer size
    RETURN_IF_FALSE(m_perMeshConstantBuffer->InitGPUBuffer(resourceManager.GetDevice(), {L"MeshPassBase_PerMeshConstantBuffer", static_cast<size_t>(64 * 1024), 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer }))
    
    return true;
}

bool glTFRenderPassMeshBase::RenderPass(glTFRenderResourceManager& resourceManager)
{
    return glTFRenderPassBase::RenderPass(resourceManager);
}

bool glTFRenderPassMeshBase::AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resourceManager, const glTFScenePrimitive& primitive)
{
    const glTFUniqueID meshID = primitive.GetID();
    if (m_meshes.find(meshID) == m_meshes.end())
    {
        // Upload vertex and index data once
        auto& vertexBuffer = m_meshes[meshID].meshVertexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        auto& indexBuffer = m_meshes[meshID].meshIndexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        auto& vertexBufferView = m_meshes[meshID].meshVertexBufferView = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
        auto& indexBufferView = m_meshes[meshID].meshIndexBufferView = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
        
        const auto& primitiveVertices = primitive.GetVertexBufferData();
        const auto& primitiveIndices = primitive.GetIndexBufferData();
        
        RHIBufferDesc vertexBufferDesc = {L"vertexBufferDefaultBuffer", primitiveVertices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        RHIBufferDesc indexBufferDesc = {L"indexBufferDefaultBuffer", primitiveIndices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    
        RETURN_IF_FALSE(vertexBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexBufferDesc ))
        RETURN_IF_FALSE(indexBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexBufferDesc ))
    
        auto vertexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        auto indexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

        RHIBufferDesc vertexUploadBufferDesc = {L"vertexBufferUploadBuffer", primitiveVertices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        RHIBufferDesc indexUploadBufferDesc = {L"indexBufferUploadBuffer", primitiveIndices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};

        RETURN_IF_FALSE(vertexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexUploadBufferDesc ))
        RETURN_IF_FALSE(indexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexUploadBufferDesc ))
        RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *vertexUploadBuffer, *vertexBuffer, primitiveVertices.data.get(), primitiveVertices.byteSize))
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *indexUploadBuffer, *indexBuffer, primitiveIndices.data.get(), primitiveIndices.byteSize))
    
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *vertexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER))
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *indexBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))
        RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
        RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))

        auto fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
        RETURN_IF_FALSE(fence->InitFence(resourceManager.GetDevice()))

        RETURN_IF_FALSE(fence->SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))
        RETURN_IF_FALSE(fence->WaitUtilSignal())
        
        vertexBufferView->InitVertexBufferView(*vertexBuffer, 0, primitive.GetVertexLayout().GetVertexStride(), primitiveVertices.byteSize);
        indexBufferView->InitIndexBufferView(*indexBuffer, 0, RHIDataFormat::R32_UINT, primitiveIndices.byteSize);

        m_meshes[meshID].meshVertexCount = primitiveVertices.VertexCount();
        m_meshes[meshID].meshIndexCount = primitiveIndices.IndexCount();
        m_meshes[meshID].materialID = primitive.GetMaterial().GetID();
    }

    // Only update when transform has changed
    m_meshes[meshID].meshTransformMatrix = primitive.GetTransformMatrix();
    
    return true;
}

bool glTFRenderPassMeshBase::RemovePrimitiveFromMeshPass(glTFUniqueID meshIDToRemove)
{
    if (auto it = (m_meshes.find(meshIDToRemove)) != m_meshes.end())
    {
        LOG_FORMAT("[DEBUG] Remove mesh id %d", meshIDToRemove)
        m_meshes.erase(it);
    }
    else
    {
        LOG_FORMAT("[WARN] Can not find meshID in Pass")
        assert(false);
    }

    return true;
}

void glTFRenderPassMeshBase::UpdateViewParameters(const glTFSceneView& view)
{
    m_constantBufferPerObject.viewProjection = view.GetViewProjectionMatrix();
}
