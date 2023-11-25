#include "glTFGraphicsPassMeshBase.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMesh.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMeshInfo.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassMeshBase::glTFGraphicsPassMeshBase()
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMesh>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshInfo>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>());
}

bool glTFGraphicsPassMeshBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMeshInfo>()->UpdateSceneMeshData(resource_manager.GetMeshManager()))
    
    return true;
}

bool glTFGraphicsPassMeshBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);

    if (!m_command_signature)
    {
        m_command_signature = RHIResourceFactory::CreateRHIResource<IRHICommandSignature>();
        m_command_signature->SetCommandSignatureDesc(resource_manager.GetMeshManager().GetCommandSignatureDesc());
        RETURN_IF_FALSE(m_command_signature->InitCommandSignature(resource_manager.GetDevice(), m_root_signature_helper.GetRootSignature()))
    }

    return true;
}

bool glTFGraphicsPassMeshBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))    

    auto& command_list = resource_manager.GetCommandListForRecord();

    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();

    if (UsingIndirectDraw())
    {
        const size_t indirect_command_count = resource_manager.GetMeshManager().GetIndirectDrawCommands().size();
        if (UsingIndirectDrawCulling())
        {
            RHIUtils::Instance().ExecuteIndirect(command_list, *m_command_signature, indirect_command_count, *resource_manager.GetMeshManager().GetCulledIndirectArgumentBuffer(), 0, *resource_manager.GetMeshManager().GetCulledIndirectArgumentBuffer(), resource_manager.GetMeshManager().GetCulledIndirectArgumentBufferCountOffset());
        }
        else
        {
            RHIUtils::Instance().ExecuteIndirect(command_list, *m_command_signature, indirect_command_count, *resource_manager.GetMeshManager().GetIndirectArgumentBuffer(), 0);    
        }
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
        
            //RETURN_IF_FALSE(BeginDrawMesh(resource_manager, mesh_data->second, instance.second.second))

            if (UsingInputLayout())
            {
                RHIUtils::Instance().SetVertexBufferView(command_list, 0, *mesh_data->second.mesh_vertex_buffer_view);
                RHIUtils::Instance().SetVertexBufferView(command_list, 1, *resource_manager.GetMeshManager().GetInstanceBufferView());    
            }
            
            RHIUtils::Instance().SetIndexBufferView(command_list, *mesh_data->second.mesh_index_buffer_view);
        
            RHIUtils::Instance().DrawIndexInstanced(command_list,
                mesh_data->second.mesh_index_count, instance.second.first,
                0, 0,
                instance.second.second);

            //RETURN_IF_FALSE(EndDrawMesh(resource_manager, mesh_data->second, instance.second))
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

    return true;
}

bool glTFGraphicsPassMeshBase::BeginDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned instance_offset)
{
    // Upload constant buffer
    ConstantBufferSceneMesh temp_mesh_data =
    {
        instance_offset
    };
        
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMesh>()->UploadAndApplyDataWithIndex(resource_manager, instance_offset, temp_mesh_data, true))
    
    return true;   
}

bool glTFGraphicsPassMeshBase::EndDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index)
{
    return true;
}

bool glTFGraphicsPassMeshBase::TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object)
{
    return true;
}

const std::vector<RHIPipelineInputLayout>& glTFGraphicsPassMeshBase::GetVertexInputLayout(
    glTFRenderResourceManager& resource_manager)
{
    return resource_manager.GetMeshManager().GetVertexInputLayout();
}
