#include "glTFGraphicsPassMeshDepth.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

bool glTFGraphicsPassMeshDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);
    GetGraphicsPipelineStateObject().BindRenderTargetFormats({&resource_manager.GetDepthRT()});
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::STATE_COMMON, RHIResourceStateType::STATE_DEPTH_READ))

    return true;
}

bool glTFGraphicsPassMeshDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::STATE_DEPTH_READ, RHIResourceStateType::STATE_DEPTH_WRITE))

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list, {}, &resource_manager.GetDepthRT()))
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list, {&resource_manager.GetDepthRT()}))

    return true;
}

bool glTFGraphicsPassMeshDepth::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PostRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::STATE_DEPTH_WRITE, RHIResourceStateType::STATE_DEPTH_READ))

    return true;
}

size_t glTFGraphicsPassMeshDepth::GetMainDescriptorHeapSize()
{
    return 1;
}
