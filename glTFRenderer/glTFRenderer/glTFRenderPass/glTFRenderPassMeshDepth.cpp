#include "glTFRenderPassMeshDepth.h"

#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

bool glTFRenderPassMeshDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupPipelineStateObject(resource_manager))

    m_pipeline_state_object->BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    
    m_pipeline_state_object->BindRenderTargets({&resource_manager.GetDepthRT()});

    m_pipeline_state_object->SetDepthStencilState(IRHIDepthStencilMode::DEPTH_WRITE);
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::DEPTH_WRITE, RHIResourceStateType::DEPTH_READ))
    
    return true;
}

bool glTFRenderPassMeshDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::DEPTH_READ, RHIResourceStateType::DEPTH_WRITE))
    
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list, {}, &resource_manager.GetDepthRT()))
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list, {&resource_manager.GetDepthRT()}))

    return true;
}

bool glTFRenderPassMeshDepth::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::PostRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(), RHIResourceStateType::DEPTH_WRITE, RHIResourceStateType::DEPTH_READ))
    
    return true;
}

size_t glTFRenderPassMeshDepth::GetMainDescriptorHeapSize()
{
    return 1;
}
