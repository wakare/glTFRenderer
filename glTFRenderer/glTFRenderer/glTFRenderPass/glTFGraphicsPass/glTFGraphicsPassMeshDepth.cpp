#include "glTFGraphicsPassMeshDepth.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

bool glTFGraphicsPassMeshDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);
    GetGraphicsPipelineStateObject().BindRenderTargetFormats({&resource_manager.GetDepthDSV()});

    return true;
}

bool glTFGraphicsPassMeshDepth::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))
    
    return true;
}

bool glTFGraphicsPassMeshDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))
    
    m_begin_rendering_info.m_render_targets = {&resource_manager.GetDepthDSV()};
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;;

    return true;
}

bool glTFGraphicsPassMeshDepth::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PostRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    resource_manager.GetDepthTextureRef().Transition(command_list, RHIResourceStateType::STATE_DEPTH_READ);
    
    return true;
}
