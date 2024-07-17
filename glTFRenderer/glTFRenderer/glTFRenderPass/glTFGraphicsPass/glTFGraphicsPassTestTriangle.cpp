#include "glTFGraphicsPassTestTriangle.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"

glTFGraphicsPassTestTriangle::glTFGraphicsPassTestTriangle()
= default;

const char* glTFGraphicsPassTestTriangle::PassName()
{
    return "TestTrianglePass";
}

bool glTFGraphicsPassTestTriangle::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager))

    return true;
}

bool glTFGraphicsPassTestTriangle::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);
    
    m_begin_rendering_info.m_render_targets = {&resource_manager.GetCurrentFrameSwapChainRTV()};
    m_begin_rendering_info.enable_depth_write = false;
    
    return true;
}

bool glTFGraphicsPassTestTriangle::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    RHIUtils::Instance().DrawInstanced(command_list, 3, 1, 0, 0);
    
    return true;
}

bool glTFGraphicsPassTestTriangle::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PostRenderPass(resource_manager))

    return true;
}

bool glTFGraphicsPassTestTriangle::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestTriangleVert.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestTriangleFrag.hlsl)", RHIShaderType::Pixel, "main");
    
    std::vector<IRHIDescriptorAllocation*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapChainRTV());
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);
    
    return true;
}
