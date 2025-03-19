#include "glTFGraphicsPassBase.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassBase::glTFGraphicsPassBase()
= default;

bool glTFGraphicsPassBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::InitPass(resource_manager))

    return true;
}

bool glTFGraphicsPassBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    const RHIViewportDesc viewport = GetViewport(resource_manager);
    RHIUtils::Instance().SetViewport(command_list, viewport);

    const RHIScissorRectDesc scissor_rect =
        {
            (unsigned)viewport.top_left_x,
            (unsigned)viewport.top_left_y,
            (unsigned)viewport.width,
            (unsigned)viewport.height
        }; 
    RHIUtils::Instance().SetScissorRect(command_list, scissor_rect);

    m_begin_rendering_info.rendering_area_offset_x = viewport.top_left_x;
    m_begin_rendering_info.rendering_area_offset_y = viewport.top_left_y;
    m_begin_rendering_info.rendering_area_width = viewport.width;
    m_begin_rendering_info.rendering_area_height = viewport.height;
    m_begin_rendering_info.clear_render_target = false;
    
    return true;
}

bool glTFGraphicsPassBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::RenderPass(resource_manager))
    
    RHIUtils::Instance().BeginRendering(resource_manager.GetCommandListForRecord(), m_begin_rendering_info);
    
    return true;
}

bool glTFGraphicsPassBase::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::PostRenderPass(resource_manager))

    RHIUtils::Instance().EndRendering(resource_manager.GetCommandListForRecord());
    
    return true;
}

RHIViewportDesc glTFGraphicsPassBase::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    return {0, 0, (float)resource_manager.GetSwapChain().GetWidth(), (float)resource_manager.GetSwapChain().GetHeight(), 0.0f, 1.0f };
}

IRHIGraphicsPipelineStateObject& glTFGraphicsPassBase::GetGraphicsPipelineStateObject() const
{
    return dynamic_cast<IRHIGraphicsPipelineStateObject&>(*m_pipeline_state_object);
}

RHICullMode glTFGraphicsPassBase::GetCullMode()
{
    return RHICullMode::NONE;
}

bool glTFGraphicsPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::SetupPipelineStateObject(resource_manager))

    // glTF using CCW as front face
    GetGraphicsPipelineStateObject().SetCullMode(GetCullMode());
    
    return true;
}
