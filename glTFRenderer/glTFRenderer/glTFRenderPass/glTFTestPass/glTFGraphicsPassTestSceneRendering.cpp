#include "glTFGraphicsPassTestSceneRendering.h"

#include "glTFRenderPass/glTFRenderMeshManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMeshInfo.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceStructuredBuffer.h"

glTFGraphicsPassTestSceneRendering::glTFGraphicsPassTestSceneRendering()
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshInfo>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>());
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
}

bool glTFGraphicsPassTestSceneRendering::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager));
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMeshInfo>()->UpdateSceneMeshData(resource_manager, resource_manager.GetMeshManager()))

    const auto& instance_buffer_data = resource_manager.GetMeshManager().GetInstanceBufferData(); 
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>()->UploadCPUBuffer(resource_manager, instance_buffer_data.data(), 0, sizeof(MeshInstanceInputData) * instance_buffer_data.size());

    GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager);
    
    return true;
}

bool glTFGraphicsPassTestSceneRendering::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupPipelineStateObject(resource_manager))
    
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestSceneRenderingVert.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestSceneRenderingFrag.hlsl)", RHIShaderType::Pixel, "main");

    auto& shader_macros = GetGraphicsPipelineStateObject().GetShaderMacros();
    shader_macros.AddMacro("ENABLE_INPUT_LAYOUT", "1");
    
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
    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);
    
    m_begin_rendering_info.m_render_targets = {&resource_manager.GetCurrentFrameSwapChainRTV(), &resource_manager.GetDepthDSV()};
    m_begin_rendering_info.enable_depth_write = true;
    m_begin_rendering_info.clear_depth = true;
    m_begin_rendering_info.clear_render_target = true;
    
    return true;
}

bool glTFGraphicsPassTestSceneRendering::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();

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
            RHIUtils::Instance().SetVertexBufferView(command_list, 0, *mesh_data->second.mesh_vertex_buffer_view);
            RHIUtils::Instance().SetVertexBufferView(command_list, 1, *resource_manager.GetMeshManager().GetInstanceBufferView());    
        }
                
        RHIUtils::Instance().SetIndexBufferView(command_list, *mesh_data->second.mesh_index_buffer_view);
        
        RHIUtils::Instance().DrawIndexInstanced(command_list,
            mesh_data->second.mesh_index_count, instance.second.first,
            0, 0,
            instance.second.second);
    }
    
    return true;
}

const RHIVertexStreamingManager& glTFGraphicsPassTestSceneRendering::GetVertexStreamingManager(
    glTFRenderResourceManager& resource_manager) const
{
    return resource_manager.GetMeshManager().GetVertexStreamingManager();
}
