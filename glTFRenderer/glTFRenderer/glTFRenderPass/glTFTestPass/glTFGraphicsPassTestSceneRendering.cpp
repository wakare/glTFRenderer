#include "glTFGraphicsPassTestSceneRendering.h"

#include "glTFRenderPass/glTFRenderMeshManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMeshInfo.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceStructuredBuffer.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIUtils.h"
#include "RHIInterface/IRHIPipelineStateObject.h"

glTFGraphicsPassTestSceneRendering::glTFGraphicsPassTestSceneRendering()
{
    
}

bool glTFGraphicsPassTestSceneRendering::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitRenderInterface(resource_manager))
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshInfo>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>());
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
    //GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager);
    
    return true;
}

bool glTFGraphicsPassTestSceneRendering::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager));
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMeshInfo>()->UpdateSceneMeshData(resource_manager, resource_manager.GetMeshManager()))
    
    m_command_signature = resource_manager.GetMeshManager().GetIndirectDrawBuilder().BuildCommandSignature(resource_manager.GetDevice(), m_root_signature_helper.GetRootSignature());
    GLTF_CHECK(m_command_signature);
    
    return true;
}

bool glTFGraphicsPassTestSceneRendering::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupPipelineStateObject(resource_manager))
    
    BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestSceneRenderingVert.hlsl)", RHIShaderType::Vertex, "main");
    BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestSceneRenderingFrag.hlsl)", RHIShaderType::Pixel, "main");

    auto& shader_macros = GetShaderMacros();
    shader_macros.AddMacro("ENABLE_INPUT_LAYOUT", m_indirect_draw ? "0" : "1");
    
    std::vector<IRHIDescriptorAllocation*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapChainRTV());
    render_targets.push_back(&resource_manager.GetDepthDSV());
    
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);
    
    return true;
}

bool glTFGraphicsPassTestSceneRendering::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager));
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RHIUtilInstanceManager::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);
    
    m_begin_rendering_info.m_render_targets = {&resource_manager.GetCurrentFrameSwapChainRTV(), &resource_manager.GetDepthDSV()};
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;

    const auto& instance_buffer_data = resource_manager.GetMeshManager().GetInstanceBufferData(); 
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>()->UploadBuffer(resource_manager, instance_buffer_data.data(), 0, sizeof(MeshInstanceInputData) * instance_buffer_data.size());

    return true;
}

bool glTFGraphicsPassTestSceneRendering::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();

    if (m_indirect_draw)
    {
        // Bind mega index buffer
        RHIUtilInstanceManager::Instance().SetIndexBufferView(command_list, *resource_manager.GetMeshManager().GetMegaIndexBufferView());
        resource_manager.GetMeshManager().GetIndirectDrawBuilder().DrawIndirect(command_list, *m_command_signature, false);
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

const RHIVertexStreamingManager& glTFGraphicsPassTestSceneRendering::GetVertexStreamingManager(
    glTFRenderResourceManager& resource_manager) const
{
    return m_indirect_draw ? m_dummy_vertex_streaming_manager : resource_manager.GetMeshManager().GetVertexStreamingManager();
}
