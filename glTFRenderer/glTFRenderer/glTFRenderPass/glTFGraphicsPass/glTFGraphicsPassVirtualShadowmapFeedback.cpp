#include "glTFGraphicsPassVirtualShadowmapFeedback.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderSystem/Shadow/ShadowRenderSystem.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

glTFGraphicsPassVirtualShadowmapFeedback::glTFGraphicsPassVirtualShadowmapFeedback(const VSMConfig& config)
    : m_config(config)
{
}

bool glTFGraphicsPassVirtualShadowmapFeedback::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceShadowMapView>(m_config.light_id));
    
    return true;
}

bool glTFGraphicsPassVirtualShadowmapFeedback::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))

        GetGraphicsPipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
        GetGraphicsPipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassVTFeedBackPS.hlsl)", RHIShaderType::Pixel, "main");

    GetGraphicsPipelineStateObject().BindRenderTargetFormats(
        {
            GetResourceDescriptor(GetVTFeedBackId(m_config.virtual_texture_id)).get(),
            &resource_manager.GetDepthDSV()
        });
    
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_DONT_CARE);
    
    return true;}

bool glTFGraphicsPassVirtualShadowmapFeedback::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    GetResourceTexture(GetVTFeedBackId(m_config.virtual_texture_id))->Transition(resource_manager.GetCommandListForRecord(), RHIResourceStateType::STATE_RENDER_TARGET);
    
    std::vector render_targets
        {
            GetResourceDescriptor(GetVTFeedBackId(m_config.virtual_texture_id)).get(),
            &resource_manager.GetDepthDSV()
        };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;
    
    return true;
}

bool glTFGraphicsPassVirtualShadowmapFeedback::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))
    int shadowmap_feedback_width, shadowmap_feedback_height;
    ShadowRenderSystem::GetVirtualShadowmapFeedbackSize(resource_manager, shadowmap_feedback_width, shadowmap_feedback_height);
    RHITextureDesc feed_back_desc = RHITextureDesc::MakeVirtualTextureFeedbackDesc(resource_manager, shadowmap_feedback_width, shadowmap_feedback_height);
    AddExportTextureResource(GetVTFeedBackId(m_config.virtual_texture_id), feed_back_desc, RHITextureDescriptorDesc
    {
        RHIDataFormat::R32G32B32A32_UINT,
        RHIResourceDimension::TEXTURE2D,
        RHIViewType::RVT_RTV
    });
    
    return true;
}

RHIViewportDesc glTFGraphicsPassVirtualShadowmapFeedback::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    int shadowmap_feedback_width, shadowmap_feedback_height;
    ShadowRenderSystem::GetVirtualShadowmapFeedbackSize(resource_manager, shadowmap_feedback_width, shadowmap_feedback_height);
    RHIViewportDesc viewport_desc{0.0f, 0.0f,
        static_cast<float>(shadowmap_feedback_width), static_cast<float>(shadowmap_feedback_height), 0.0f, 1.0f};

    return viewport_desc;
}
