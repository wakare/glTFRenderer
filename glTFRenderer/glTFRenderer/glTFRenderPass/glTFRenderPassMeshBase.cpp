#include "glTFRenderPassMeshBase.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassMeshBase::glTFRenderPassMeshBase()
    : glTFRenderPassInterfaceSceneView(MeshBasePass_RootParameter_SceneView_CBV, MeshBasePass_SceneView_CBV_Register)
    , glTFRenderPassInterfaceSceneMesh(
        MeshBasePass_RootParameter_SceneMesh_CBV,
        MeshBasePass_SceneMesh_CBV_Register,
        MeshBasePass_RootParameter_SceneMesh_SRV,
        MeshBasePass_SceneMesh_SRV_Register,
        4)
    , m_base_pass_color_render_target(nullptr)
    , m_base_pass_normal_render_target(nullptr)
{
}

bool glTFRenderPassMeshBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE (glTFRenderPassBase::InitPass(resource_manager))
    
    return true;
}

bool glTFRenderPassMeshBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::RenderPass(resource_manager))

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(resource_manager.GetCommandList(),
        {m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get()}, &resource_manager.GetDepthRT()))

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(resource_manager.GetCommandList(),
        {m_base_pass_color_render_target.get(),m_base_pass_normal_render_target.get(), &resource_manager.GetDepthRT()}))
    RHIUtils::Instance().SetPrimitiveTopology( resource_manager.GetCommandList(), RHIPrimitiveTopologyType::TRIANGLELIST);

    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::ApplyInterface(resource_manager))

    for (const auto& mesh : m_meshes)
    {
        const glTFUniqueID meshID = mesh.first;
        RETURN_IF_FALSE(BeginDrawMesh(resource_manager, meshID))
        
        // Upload constant buffer
        RETURN_IF_FALSE(UpdateSceneMeshData(
            {
                mesh.second.meshTransformMatrix,
                glm::transpose(glm::inverse(mesh.second.meshTransformMatrix)),
                mesh.second.using_normal_mapping
            }))
        
        //glTFRenderPassInterfaceSceneMesh::ApplyInterface(resourceManager, meshID, MeshBasePass_RootParameter_SceneMesh);
        glTFRenderPassInterfaceSceneMesh::ApplyInterface(resource_manager, 0);
        
        RHIUtils::Instance().SetVertexBufferView(resource_manager.GetCommandList(), *mesh.second.mesh_vertex_buffer_view);
        RHIUtils::Instance().SetIndexBufferView(resource_manager.GetCommandList(), *mesh.second.mesh_index_buffer_view);
        
        RHIUtils::Instance().DrawIndexInstanced(resource_manager.GetCommandList(),
            mesh.second.mesh_index_count, 1, 0, 0, 0);

        RETURN_IF_FALSE(EndDrawMesh(resource_manager, mesh.first))
    }
    
    return true;
}

bool glTFRenderPassMeshBase::AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive& primitive)
{
    const glTFUniqueID mesh_ID = primitive.GetID();
    if (m_meshes.find(mesh_ID) == m_meshes.end())
    {
        // Upload vertex and index data once
        const auto& vertex_buffer = m_meshes[mesh_ID].mesh_vertex_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto& index_buffer = m_meshes[mesh_ID].mesh_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto& vertex_buffer_view = m_meshes[mesh_ID].mesh_vertex_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
        const auto& index_buffer_view = m_meshes[mesh_ID].mesh_index_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
        
        const auto& primitive_vertices = primitive.GetVertexBufferData();
        const auto& primitive_indices = primitive.GetIndexBufferData();

        const RHIBufferDesc vertex_buffer_desc = {L"vertexBufferDefaultBuffer", primitive_vertices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        const RHIBufferDesc index_buffer_desc = {L"indexBufferDefaultBuffer", primitive_indices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    
        RETURN_IF_FALSE(vertex_buffer->InitGPUBuffer(resource_manager.GetDevice(), vertex_buffer_desc ))
        RETURN_IF_FALSE(index_buffer->InitGPUBuffer(resource_manager.GetDevice(), index_buffer_desc ))

        const auto vertex_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto index_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

        const RHIBufferDesc vertex_upload_buffer_desc = {L"vertexBufferUploadBuffer", primitive_vertices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        const RHIBufferDesc index_upload_buffer_desc = {L"indexBufferUploadBuffer", primitive_indices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};

        RETURN_IF_FALSE(vertex_upload_buffer->InitGPUBuffer(resource_manager.GetDevice(), vertex_upload_buffer_desc ))
        RETURN_IF_FALSE(index_upload_buffer->InitGPUBuffer(resource_manager.GetDevice(), index_upload_buffer_desc ))
        RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resource_manager.GetCommandList(), resource_manager.GetCurrentFrameCommandAllocator()))
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resource_manager.GetCommandList(), *vertex_upload_buffer, *vertex_buffer, primitive_vertices.data.get(), primitive_vertices.byteSize))
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(resource_manager.GetCommandList(), *index_upload_buffer, *index_buffer, primitive_indices.data.get(), primitive_indices.byteSize))
    
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resource_manager.GetCommandList(), *vertex_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER))
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(resource_manager.GetCommandList(), *index_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))
        RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resource_manager.GetCommandList()))
        RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resource_manager.GetCommandList(),resource_manager.GetCommandQueue()))

        const auto fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
        RETURN_IF_FALSE(fence->InitFence(resource_manager.GetDevice()))

        RETURN_IF_FALSE(fence->SignalWhenCommandQueueFinish(resource_manager.GetCommandQueue()))
        RETURN_IF_FALSE(fence->WaitUtilSignal())
        
        vertex_buffer_view->InitVertexBufferView(*vertex_buffer, 0, primitive.GetVertexLayout().GetVertexStrideInBytes(), primitive_vertices.byteSize);
        index_buffer_view->InitIndexBufferView(*index_buffer, 0, primitive_indices.elementType == IndexBufferElementType::UNSIGNED_INT ? RHIDataFormat::R32_UINT : RHIDataFormat::R16_UINT, primitive_indices.byteSize);

        m_meshes[mesh_ID].mesh_vertex_count = primitive_vertices.vertex_count;
        m_meshes[mesh_ID].mesh_index_count = primitive_indices.index_count;
        m_meshes[mesh_ID].material_id = primitive.HasMaterial() ? primitive.GetMaterial().GetID() : glTFUniqueIDInvalid;
        m_meshes[mesh_ID].using_normal_mapping = primitive.HasNormalMapping();
    }

    // Only update when transform has changed
    m_meshes[mesh_ID].meshTransformMatrix = primitive.GetTransformMatrix();
    
    return true; 
}

bool glTFRenderPassMeshBase::RemovePrimitiveFromMeshPass(glTFUniqueID mesh_id_to_remove)
{
    if (auto it = (m_meshes.find(mesh_id_to_remove)) != m_meshes.end())
    {
        LOG_FORMAT("[DEBUG] Remove mesh id %d", mesh_id_to_remove)
        m_meshes.erase(it);
    }
    else
    {
        LOG_FORMAT("[WARN] Can not find meshID in Pass")
        assert(false);
    }

    return true;
}

bool glTFRenderPassMeshBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::SetupRootSignature(*m_root_signature))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneMesh::SetupRootSignature(*m_root_signature))
    
    return true;
}

bool glTFRenderPassMeshBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupPipelineStateObject(resource_manager))
    
    IRHIRenderTargetDesc render_target_base_color_desc;
    render_target_base_color_desc.width = resource_manager.GetSwapchain().GetWidth();
    render_target_base_color_desc.height = resource_manager.GetSwapchain().GetHeight();
    render_target_base_color_desc.name = "BasePassColor";
    render_target_base_color_desc.isUAV = true;
    render_target_base_color_desc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_base_pass_color_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_base_color_desc);

    IRHIRenderTargetDesc render_target_normal_desc;
    render_target_normal_desc.width = resource_manager.GetSwapchain().GetWidth();
    render_target_normal_desc.height = resource_manager.GetSwapchain().GetHeight();
    render_target_normal_desc.name = "BasePassNormal";
    render_target_normal_desc.isUAV = true;
    render_target_normal_desc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    m_base_pass_normal_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_normal_desc);

    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassColor", m_base_pass_color_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassNormal", m_base_pass_normal_render_target);
    
    m_pipeline_state_object->BindRenderTargets({m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get(), &resource_manager.GetDepthRT()});

    auto& shader_macros = m_pipeline_state_object->GetShaderMacros();
    glTFRenderPassInterfaceSceneView::UpdateShaderCompileDefine(shader_macros);
    glTFRenderPassInterfaceSceneMesh::UpdateShaderCompileDefine(shader_macros);
    
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

std::vector<RHIPipelineInputLayout> glTFRenderPassMeshBase::GetVertexInputLayout()
{
    return m_vertex_input_layouts;
}

bool glTFRenderPassMeshBase::TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object)
{
    const glTFScenePrimitive* primitive = dynamic_cast<const glTFScenePrimitive*>(&object);
    
    if (!primitive || !primitive->IsVisible())
    {
        return false;
    }

    return AddOrUpdatePrimitiveToMeshPass(resourceManager, *primitive) &&
        primitive->HasMaterial() && ProcessMaterial(resourceManager, primitive->GetMaterial());
}

bool glTFRenderPassMeshBase::ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout)
{
	m_vertex_input_layouts.clear();
    
    unsigned vertex_layout_offset = 0;
    for (const auto& vertex_layout : source_vertex_layout.elements)
    {
        switch (vertex_layout.type)
        {
        case VertexLayoutType::POSITION:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(POSITION), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset});
            }
            break;
        case VertexLayoutType::NORMAL:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset});
            }
            break;
        case VertexLayoutType::TANGENT:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32A32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT), 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset});
            }
            break;
        case VertexLayoutType::TEXCOORD_0:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD), 0, RHIDataFormat::R32G32_FLOAT, vertex_layout_offset});
            }
            break;
            // TODO: Handle TEXCOORD_1?
        }

        vertex_layout_offset += vertex_layout.byte_size;   
    }

    return true;
}
