#include "glTFRenderPassMeshDepth.h"

#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

bool glTFRenderPassMeshDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().SetDepthStencilState(IRHIDepthStencilMode::DEPTH_WRITE);
    GetGraphicsPipelineStateObject().BindRenderTargets({&resource_manager.GetDepthRT()});
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::DEPTH_WRITE, RHIResourceStateType::DEPTH_READ))
    
    return true;
}

bool glTFRenderPassMeshDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::DEPTH_READ, RHIResourceStateType::DEPTH_WRITE))
    
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list, {}, &resource_manager.GetDepthRT()))
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list, {&resource_manager.GetDepthRT()}))

    return true;
}

bool glTFRenderPassMeshDepth::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PostRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::DEPTH_WRITE, RHIResourceStateType::DEPTH_READ))
    
    return true;
}

size_t glTFRenderPassMeshDepth::GetMainDescriptorHeapSize()
{
    return 1;
}
