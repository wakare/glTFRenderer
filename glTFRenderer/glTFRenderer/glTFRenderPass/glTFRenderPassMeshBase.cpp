#include "glTFRenderPassMeshBase.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassMeshBase::glTFRenderPassMeshBase()
    : glTFRenderPassInterfaceSceneView(MeshBasePass_RootParameter_SceneView, MeshBasePass_RootParameter_SceneView)
    , glTFRenderPassInterfaceSceneMesh(MeshBasePass_RootParameter_SceneMesh, MeshBasePass_RootParameter_SceneMesh)
    , m_basePassColorRenderTarget(nullptr)
    , m_basePassNormalRenderTarget(nullptr)
{
}

bool glTFRenderPassMeshBase::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE (glTFRenderPassBase::InitPass(resourceManager))
    
    return true;
}

bool glTFRenderPassMeshBase::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::RenderPass(resourceManager))

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().BindRenderTarget(resourceManager.GetCommandList(),
        {m_basePassColorRenderTarget.get(), m_basePassNormalRenderTarget.get()}, &resourceManager.GetDepthRT()))

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(),
        {m_basePassColorRenderTarget.get(),m_basePassNormalRenderTarget.get(), &resourceManager.GetDepthRT()}))
    RHIUtils::Instance().SetPrimitiveTopology( resourceManager.GetCommandList(), RHIPrimitiveTopologyType::TRIANGLELIST);

    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::ApplyInterface(resourceManager, MeshBasePass_RootParameter_SceneView))

    for (const auto& mesh : m_meshes)
    {
        const glTFUniqueID meshID = mesh.first;
        RETURN_IF_FALSE(BeginDrawMesh(resourceManager, meshID))
        
        // Upload constant buffer
        RETURN_IF_FALSE(UpdateSceneMeshData({mesh.second.meshTransformMatrix, glm::transpose(glm::inverse(mesh.second.meshTransformMatrix))}))
        glTFRenderPassInterfaceSceneMesh::ApplyInterface(resourceManager, meshID, MeshBasePass_RootParameter_SceneMesh);
        
        RHIUtils::Instance().SetVertexBufferView(resourceManager.GetCommandList(), *mesh.second.meshVertexBufferView);
        RHIUtils::Instance().SetIndexBufferView(resourceManager.GetCommandList(), *mesh.second.meshIndexBufferView);
        
        RHIUtils::Instance().DrawIndexInstanced(resourceManager.GetCommandList(),
            mesh.second.meshIndexCount, 1, 0, 0, 0);

        RETURN_IF_FALSE(EndDrawMesh(resourceManager, mesh.first))
    }
    
    return true;
}

bool glTFRenderPassMeshBase::AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resourceManager, const glTFScenePrimitive& primitive)
{
    const glTFUniqueID meshID = primitive.GetID();
    if (m_meshes.find(meshID) == m_meshes.end())
    {
        // Upload vertex and index data once
        const auto& vertex_buffer = m_meshes[meshID].meshVertexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto& index_buffer = m_meshes[meshID].meshIndexBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto& vertex_buffer_view = m_meshes[meshID].meshVertexBufferView = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
        const auto& index_buffer_view = m_meshes[meshID].meshIndexBufferView = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
        
        const auto& primitive_vertices = primitive.GetVertexBufferData();
        const auto& primitive_indices = primitive.GetIndexBufferData();

        const RHIBufferDesc vertex_buffer_desc = {L"vertexBufferDefaultBuffer", primitive_vertices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        const RHIBufferDesc index_buffer_desc = {L"indexBufferDefaultBuffer", primitive_indices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    
        RETURN_IF_FALSE(vertex_buffer->InitGPUBuffer(resourceManager.GetDevice(), vertex_buffer_desc ))
        RETURN_IF_FALSE(index_buffer->InitGPUBuffer(resourceManager.GetDevice(), index_buffer_desc ))

        const auto vertexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto indexUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

        const RHIBufferDesc vertexUploadBufferDesc = {L"vertexBufferUploadBuffer", primitive_vertices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        const RHIBufferDesc indexUploadBufferDesc = {L"indexBufferUploadBuffer", primitive_indices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};

        RETURN_IF_FALSE(vertexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), vertexUploadBufferDesc ))
        RETURN_IF_FALSE(indexUploadBuffer->InitGPUBuffer(resourceManager.GetDevice(), indexUploadBufferDesc ))
        RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *vertexUploadBuffer, *vertex_buffer, primitive_vertices.data.get(), primitive_vertices.byteSize))
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resourceManager.GetCommandList(), *indexUploadBuffer, *index_buffer, primitive_indices.data.get(), primitive_indices.byteSize))
    
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *vertex_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER))
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resourceManager.GetCommandList(), *index_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))
        RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
        RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))

        const auto fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
        RETURN_IF_FALSE(fence->InitFence(resourceManager.GetDevice()))

        RETURN_IF_FALSE(fence->SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))
        RETURN_IF_FALSE(fence->WaitUtilSignal())
        
        vertex_buffer_view->InitVertexBufferView(*vertex_buffer, 0, primitive.GetVertexLayout().GetVertexStride(), primitive_vertices.byteSize);
        index_buffer_view->InitIndexBufferView(*index_buffer, 0, primitive_indices.elementType == IndexBufferElementType::UNSIGNED_INT ? RHIDataFormat::R32_UINT : RHIDataFormat::R16_UINT, primitive_indices.byteSize);

        m_meshes[meshID].meshVertexCount = primitive_vertices.vertexCount;
        m_meshes[meshID].meshIndexCount = primitive_indices.indexCount;
        m_meshes[meshID].materialID = primitive.HasMaterial() ? primitive.GetMaterial().GetID() : glTFUniqueIDInvalid;
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

bool glTFRenderPassMeshBase::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::SetupRootSignature(*m_root_signature))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneMesh::SetupRootSignature(*m_root_signature))
    
    return true;
}

bool glTFRenderPassMeshBase::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    IRHIRenderTargetDesc RenderTargetBaseColorDesc;
    RenderTargetBaseColorDesc.width = resourceManager.GetSwapchain().GetWidth();
    RenderTargetBaseColorDesc.height = resourceManager.GetSwapchain().GetHeight();
    RenderTargetBaseColorDesc.name = "BasePassColor";
    RenderTargetBaseColorDesc.isUAV = true;
    RenderTargetBaseColorDesc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_basePassColorRenderTarget = resourceManager.GetRenderTargetManager().CreateRenderTarget(
        resourceManager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RenderTargetBaseColorDesc);

    IRHIRenderTargetDesc RenderTargetNormalDesc;
    RenderTargetNormalDesc.width = resourceManager.GetSwapchain().GetWidth();
    RenderTargetNormalDesc.height = resourceManager.GetSwapchain().GetHeight();
    RenderTargetNormalDesc.name = "BasePassNormal";
    RenderTargetNormalDesc.isUAV = true;
    RenderTargetNormalDesc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    m_basePassNormalRenderTarget = resourceManager.GetRenderTargetManager().CreateRenderTarget(
        resourceManager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RenderTargetNormalDesc);

    resourceManager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassColor", m_basePassColorRenderTarget);
    resourceManager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassNormal", m_basePassNormalRenderTarget);
    
    m_pipeline_state_object->BindRenderTargets({m_basePassColorRenderTarget.get(), m_basePassNormalRenderTarget.get(), &resourceManager.GetDepthRT()});

    auto& shaderMacros = m_pipeline_state_object->GetShaderMacros();
    glTFRenderPassInterfaceSceneView::UpdateShaderCompileDefine(shaderMacros);
    glTFRenderPassInterfaceSceneMesh::UpdateShaderCompileDefine(shaderMacros);
    
    return true;
}

bool glTFRenderPassMeshBase::BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID)
{
    return true;   
}

bool glTFRenderPassMeshBase::EndDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID)
{
    return true;
}

bool glTFRenderPassMeshBase::TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object)
{
    const glTFScenePrimitive* primitive = dynamic_cast<const glTFScenePrimitive*>(&object);
    
    if (!primitive)
    {
        return false;
    }

    return AddOrUpdatePrimitiveToMeshPass(resourceManager, *primitive) &&
        primitive->HasMaterial() && ProcessMaterial(resourceManager, primitive->GetMaterial());
}
