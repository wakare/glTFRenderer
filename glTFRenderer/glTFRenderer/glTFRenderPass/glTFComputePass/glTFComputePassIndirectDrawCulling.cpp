#include "glTFComputePassIndirectDrawCulling.h"

glTFComputePassIndirectDrawCulling::glTFComputePassIndirectDrawCulling()
    : m_dispatch_count()
{
}

const char* glTFComputePassIndirectDrawCulling::PassName()
{
    return "IndirectDrawCullingComputePass";
}

bool glTFComputePassIndirectDrawCulling::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    // Generate indirect draw command buffer
    
    return true;
}

bool glTFComputePassIndirectDrawCulling::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    return true;
}

DispatchCount glTFComputePassIndirectDrawCulling::GetDispatchCount() const
{
    return m_dispatch_count;
}

bool glTFComputePassIndirectDrawCulling::InitVertexAndInstanceBufferForFrame(
    glTFRenderResourceManager& resource_manager)
{

    return true;
}
