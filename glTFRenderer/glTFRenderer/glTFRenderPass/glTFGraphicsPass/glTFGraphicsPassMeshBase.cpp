#include "glTFGraphicsPassMeshBase.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMeshInfo.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceStructuredBuffer.h"
#include "RHIResourceFactoryImpl.hpp"

bool glTFGraphicsPassMeshBase::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitRenderInterface(resource_manager))
    
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshInfo>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>());
    
    return true;
}

bool glTFGraphicsPassMeshBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMeshInfo>()->UpdateSceneMeshData(resource_manager, resource_manager.GetMeshManager()))

    const auto& instance_buffer_data = resource_manager.GetMeshManager().GetInstanceBufferData(); 
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>()->UploadBuffer(resource_manager, instance_buffer_data.data(), 0, sizeof(MeshInstanceInputData) * instance_buffer_data.size());
    
    m_command_signature = resource_manager.GetMeshManager().GetIndirectDrawBuilder().BuildCommandSignature(resource_manager.GetDevice(), m_root_signature_helper.GetRootSignature());
    GLTF_CHECK(m_command_signature);
    
    return true;
}

bool glTFGraphicsPassMeshBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    RHIUtilInstanceManager::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);

    return true;
}

bool glTFGraphicsPassMeshBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))    

        auto& command_list = resource_manager.GetCommandListForRecord();

    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();

    if (UsingIndirectDraw())
    {
        // Bind mega index buffer
        RHIUtilInstanceManager::Instance().SetIndexBufferView(command_list, *resource_manager.GetMeshManager().GetMegaIndexBufferView());
        resource_manager.GetMeshManager().GetIndirectDrawBuilder().DrawIndirect(command_list, *m_command_signature, UsingIndirectDrawCulling());
    }
    else
    {
        for (const auto& instance : resource_manager.GetMeshManager().GetInstanceDrawInfo())
        {
            const auto& mesh_data = mesh_render_resources.find(instance.first);
            if (mesh_data == mesh_render_resources.end())
            {
                // No valid instance data exists..
                continue;
            }
        
            if (!mesh_data->second.mesh->IsVisible())
            {
                continue;
            }
        
            if (UsingInputLayout())
            {
                RHIUtilInstanceManager::Instance().SetVertexBufferView(command_list, 0, *mesh_data->second.mesh_vertex_buffer_view);
                RHIUtilInstanceManager::Instance().SetVertexBufferView(command_list, 1, *resource_manager.GetMeshManager().GetInstanceBufferView());    
            }
                
            RHIUtilInstanceManager::Instance().SetIndexBufferView(command_list, *mesh_data->second.mesh_index_buffer_view);
        
            RHIUtilInstanceManager::Instance().DrawIndexInstanced(command_list,
                mesh_data->second.mesh_index_count, instance.second.first,
                0, 0,
                instance.second.second);
        }
    }
    
    return true;
}

bool glTFGraphicsPassMeshBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupRootSignature(resource_manager))

    return true;
}

bool glTFGraphicsPassMeshBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupPipelineStateObject(resource_manager))
    
    auto& shader_macros = GetGraphicsPipelineStateObject().GetShaderMacros();
    shader_macros.AddMacro("ENABLE_INPUT_LAYOUT", UsingInputLayout() ? "1" : "0");

    if (UsingInputLayout())
    {
        GetGraphicsPipelineStateObject().SetInputLayouts(GetVertexStreamingManager(resource_manager).GetVertexAttributes());    
    }
    
    return true;
}

bool glTFGraphicsPassMeshBase::UsingIndirectDraw() const
{
    return m_indirect_draw;
}

bool glTFGraphicsPassMeshBase::UsingIndirectDrawCulling() const
{
    return false;
}

bool glTFGraphicsPassMeshBase::UsingInputLayout() const
{
    return false;
}

const RHIVertexStreamingManager& glTFGraphicsPassMeshBase::GetVertexStreamingManager(glTFRenderResourceManager& resource_manager) const
{
    return resource_manager.GetMeshManager().GetVertexStreamingManager();
}

bool glTFGraphicsPassMeshBase::TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object)
{
    return true;
}

bool glTFGraphicsPassMeshBase::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::UpdateGUIWidgets())

    ImGui::Checkbox("Indirect Draw", &m_indirect_draw);
    
    return true;
}

RHICullMode glTFGraphicsPassMeshBase::GetCullMode()
{
    return RHICullMode::CW;
}

bool glTFGraphicsPassMeshBaseSceneView::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    
    return true;
}

bool glTFGraphicsPassMeshBaseSceneView::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::UpdateGUIWidgets())
    ImGui::Checkbox("Indirect Draw Culling", &m_use_indirect_culling);
    
    return true;
}

bool glTFGraphicsPassMeshBaseSceneView::UsingIndirectDrawCulling() const
{
    return m_use_indirect_culling;
}
