#include "glTFGraphicsPassTestTriangleBase.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"

bool glTFGraphicsPassTestTriangleBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);
    
    m_begin_rendering_info.m_render_targets = {&resource_manager.GetCurrentFrameSwapChainRTV()};
    m_begin_rendering_info.enable_depth_write = false;

    return true;
}
