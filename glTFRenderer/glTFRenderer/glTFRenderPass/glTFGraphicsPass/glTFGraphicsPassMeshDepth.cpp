#include "glTFGraphicsPassMeshDepth.h"
#include "RHIUtils.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIDescriptorManager.h"

bool glTFGraphicsPassMeshDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))

    BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);
    GetGraphicsPipelineStateObject().BindRenderTargetFormats({&resource_manager.GetDepthDSV()});

    return true;
}

bool glTFGraphicsPassMeshDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))
    
    m_begin_rendering_info.m_render_targets = {&resource_manager.GetDepthDSV()};
    m_begin_rendering_info.enable_depth_write = true;

    return true;
}

bool glTFGraphicsPassMeshDepth::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PostRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    resource_manager.GetDepthTextureRef().Transition(command_list, RHIResourceStateType::STATE_DEPTH_READ);
    
    return true;
}
