#include "glTFRenderPassMeshBase.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassMeshBase::glTFRenderPassMeshBase()
    : glTFRenderPassInterfaceSceneView(MeshBasePass_RootParameter_SceneView, MeshBasePass_RootParameter_SceneView)
    , glTFRenderPassInterfaceSceneMesh(MeshBasePass_RootParameter_SceneMesh, MeshBasePass_RootParameter_SceneMesh)
    , m_basePassColorRenderTarget(nullptr)
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
        m_basePassColorRenderTarget.get(), 1, &resourceManager.GetDepthRT()))

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(), m_basePassColorRenderTarget.get(), 1))
    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(), &resourceManager.GetDepthRT(), 1))
    RHIUtils::Instance().SetPrimitiveTopology( resourceManager.GetCommandList(), RHIPrimitiveTopologyType::TRIANGLELIST);

    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::ApplyInterface(resourceManager, MeshBasePass_RootParameter_SceneView))

    for (const auto& mesh : m_meshes)
    {
        glTFUniqueID meshID = mesh.first;
        RETURN_IF_FALSE(BeginDrawMesh(resourceManager, meshID))
        
        // Upload constant buffer
        RETURN_IF_FALSE(UpdateSceneMeshData({mesh.second.meshTransformMatrix}))
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
        indexBufferView->InitIndexBufferView(*indexBuffer, 0, primitiveIndices.elementType == IndexBufferElementType::UNSIGNED_INT ? RHIDataFormat::R32_UINT : RHIDataFormat::R16_UINT, primitiveIndices.byteSize);

        m_meshes[meshID].meshVertexCount = primitiveVertices.vertexCount;
        m_meshes[meshID].meshIndexCount = primitiveIndices.indexCount;
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
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::SetupRootSignature(*m_rootSignature))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneMesh::SetupRootSignature(*m_rootSignature))
    
    return true;
}

bool glTFRenderPassMeshBase::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    std::vector<IRHIRenderTarget*> allRts;

    IRHIRenderTargetDesc RenderTargetDesc;
    RenderTargetDesc.width = resourceManager.GetSwapchain().GetWidth();
    RenderTargetDesc.height = resourceManager.GetSwapchain().GetHeight();
    RenderTargetDesc.name = "BasePassColor";
    RenderTargetDesc.isUAV = true;
    RenderTargetDesc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_basePassColorRenderTarget = resourceManager.GetRenderTargetManager().CreateRenderTarget(
        resourceManager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RenderTargetDesc);
    allRts.push_back(m_basePassColorRenderTarget.get());
    allRts.push_back(&resourceManager.GetDepthRT());

    resourceManager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassColor", m_basePassColorRenderTarget);
    
    m_pipelineStateObject->BindRenderTargets(allRts);

    auto& shaderMacros = m_pipelineStateObject->GetShaderMacros();
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
