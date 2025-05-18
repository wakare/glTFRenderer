#include "glTFRayTracingPassBase.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "RHIResourceFactory.h"
#include "RHIUtils.h"

bool glTFRayTracingPassBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::InitPass(resource_manager))
    
    return true;
}

bool glTFRayTracingPassBase::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupRootSignature(resourceManager))
    
    return true;
}

bool glTFRayTracingPassBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::PreRenderPass(resource_manager))
    
    return true;
}

bool glTFRayTracingPassBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    const auto trace_count = GetTraceCount();
    RETURN_IF_FALSE(RHIUtilInstanceManager::Instance().TraceRay(command_list, GetShaderTable(), trace_count.X, trace_count.Y, trace_count.Z))
    
    return true;
}

bool glTFRayTracingPassBase::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::PostRenderPass(resource_manager))

    return true;
}

IRHIRayTracingPipelineStateObject& glTFRayTracingPassBase::GetRayTracingPipelineStateObject() const
{
    GLTF_CHECK(m_pipeline_state_object.get());
    
    return dynamic_cast<IRHIRayTracingPipelineStateObject&>(*m_pipeline_state_object);
}

bool glTFRayTracingPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupPipelineStateObject(resource_manager))

    return true;
}
