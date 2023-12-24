#include "glTFComputePassRayTracingPostprocess.h"

glTFComputePassRayTracingPostprocess::glTFComputePassRayTracingPostprocess()
    : m_dispatch_count({0, 0, 0})
{
}

const char* glTFComputePassRayTracingPostprocess::PassName()
{
    return "RayTracingPostProcessPass";
}

bool glTFComputePassRayTracingPostprocess::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    return true;
}

bool glTFComputePassRayTracingPostprocess::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    return true;
}

bool glTFComputePassRayTracingPostprocess::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))
    
    return true;
}

DispatchCount glTFComputePassRayTracingPostprocess::GetDispatchCount() const
{
    return m_dispatch_count;
}

bool glTFComputePassRayTracingPostprocess::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
    const glTFSceneObjectBase& object)
{
    RETURN_IF_FALSE(glTFComputePassBase::TryProcessSceneObject(resource_manager, object))
    
    return true;
}

bool glTFComputePassRayTracingPostprocess::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::FinishProcessSceneObject(resource_manager))

    return true;
}

bool glTFComputePassRayTracingPostprocess::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))

    return true;
}

bool glTFComputePassRayTracingPostprocess::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    return true;
}