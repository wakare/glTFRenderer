#include "glTFGraphicsPassTestTriangle.h"

#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

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
    
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list,
        {&resource_manager.GetCurrentFrameSwapchainRT()}, nullptr))

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list,
        {&resource_manager.GetCurrentFrameSwapchainRT()}))

    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);
    
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
    
    std::vector<IRHIRenderTarget*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapchainRT());
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);
    
    return true;
}
