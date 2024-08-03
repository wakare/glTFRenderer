#include "glTFGraphicsPassTestTriangleSimplest.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

const char* glTFGraphicsPassTestTriangleSimplest::PassName()
{
    return "TestTrianglePass";
}

bool glTFGraphicsPassTestTriangleSimplest::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    RHIUtils::Instance().DrawInstanced(command_list, 3, 1, 0, 0);
    
    return true;
}


bool glTFGraphicsPassTestTriangleSimplest::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
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
