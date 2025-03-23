#include "glTFRenderInterfaceShadowmap.h"

#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderInterfaceViewBase.h"

glTFRenderInterfaceShadowmap::glTFRenderInterfaceShadowmap(ShadowmapType type)
    : m_type(type)
{
    if (m_type == ShadowmapType::DRAW_SHADOW_MAP)
    {
           
    }
    else if (m_type == ShadowmapType::READ_SHADOW_MAP)
    {
        
    }
}

bool glTFRenderInterfaceShadowmap::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    if (m_type == ShadowmapType::DRAW_SHADOW_MAP)
    {
        
    }
    else if (m_type == ShadowmapType::READ_SHADOW_MAP)
    {
        
    }
    
    return true;
}

bool glTFRenderInterfaceShadowmap::ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager,
    IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater,
    unsigned frame_index)
{
    RETURN_IF_FALSE(glTFRenderInterfaceBase::ApplyInterfaceImpl(resource_manager, command_list, pipeline_type, descriptor_updater, frame_index));

    return true;
}