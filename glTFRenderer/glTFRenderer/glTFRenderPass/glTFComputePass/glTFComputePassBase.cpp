#include "glTFComputePassBase.h"

#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool glTFComputePassBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::InitPass(resource_manager))

    return true;
}

bool glTFComputePassBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFComputePassBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::PreRenderPass(resource_manager))
    
    return true;
}

bool glTFComputePassBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    const auto& DispatchCount = GetDispatchCount();
    
    RETURN_IF_FALSE(RHIUtils::Instance().Dispatch(command_list, DispatchCount.X, DispatchCount.Y, DispatchCount.Z))
    
    return true;
}

bool glTFComputePassBase::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::PostRenderPass(resource_manager))

    return true;
}

IRHIComputePipelineStateObject& glTFComputePassBase::GetComputePipelineStateObject() const
{
    return dynamic_cast<IRHIComputePipelineStateObject&>(*m_pipeline_state_object);
}

bool glTFComputePassBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupPipelineStateObject(resource_manager))
    
    return true;
}
