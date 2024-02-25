#include "glTFGraphicsPassBase.h"
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
    const RHIViewportDesc viewport = {0, 0, (float)resource_manager.GetSwapchain().GetWidth(), (float)resource_manager.GetSwapchain().GetHeight(), 0.0f, 1.0f };
    RHIUtils::Instance().SetViewport(command_list, viewport);

    const RHIScissorRectDesc scissorRect = {0, 0, resource_manager.GetSwapchain().GetWidth(), resource_manager.GetSwapchain().GetHeight() }; 
    RHIUtils::Instance().SetScissorRect(command_list, scissorRect);

    return true;
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

    RETURN_IF_FALSE(GetGraphicsPipelineStateObject().BindSwapChain(resource_manager.GetSwapchain()))

    // glTF using CCW as front face
    GetGraphicsPipelineStateObject().SetCullMode(GetCullMode());
    
    return true;
}
