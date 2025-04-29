#include "glTFGraphicsPassMeshVTFeedback.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceVT.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

struct FeedbackConfig
{
    inline static std::string Name = "FEEDBACK_CONFIG_REGISTER_INDEX";
    unsigned mipmap_offset;
    unsigned padding[3];
};

struct ShadowInfo
{
    inline static std::string Name = "SHADOWMAP_INFO_REGISTER_INDEX";
    unsigned vt_id;
    unsigned shadowmap_vt_size;
    unsigned shadowmap_vt_page_size;
    unsigned shadowmap_page_table_texture_size;
};

glTFGraphicsPassMeshVTFeedback::glTFGraphicsPassMeshVTFeedback(unsigned virtual_texture_id, bool shadowmap)
    : m_virtual_texture_id(virtual_texture_id)
    , m_shadowmap(shadowmap)
    , m_feedback_mipmap_offset(0)
{
}

bool glTFGraphicsPassMeshVTFeedback::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::UpdateGUIWidgets())

    ImGui::SliderInt("Mipmap offset", &m_feedback_mipmap_offset, 0, 3);
    
    return true;
}

RHIViewportDesc glTFGraphicsPassMeshVTFeedback::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    const auto& logical_texture_info = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetLogicalTextureInfo(m_virtual_texture_id);
    const auto& vt_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetVTFeedbackTextureSize(logical_texture_info);
    
    return {0, 0, static_cast<float>(vt_size.first), static_cast<float>(vt_size.second), 0.0f, 1.0f};
}

bool glTFGraphicsPassMeshVTFeedback::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<FeedbackConfig>>());
    if (m_shadowmap)
    {
        const auto& logical_texture = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetLogicalTextureInfo(m_virtual_texture_id);
        AddRenderInterface(std::make_shared<glTFRenderInterfaceShadowMapView>(dynamic_cast<const VTShadowmapLogicalTexture*>(&logical_texture)->GetLightId()));
        AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<ShadowInfo>>());
    }
    else
    {
        AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
        AddRenderInterface(std::make_shared<glTFRenderInterfaceVT>(InterfaceVTType::RENDER_VT_FEEDBACK));
        AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());    
    }
    
    return true;
}

bool glTFGraphicsPassMeshVTFeedback::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))

    if (m_shadowmap)
    {
        auto& virtual_texture_info = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetLogicalTextureInfo(m_virtual_texture_id);
        ShadowInfo shadow_info;
        shadow_info.vt_id = m_virtual_texture_id;
        shadow_info.shadowmap_vt_size = virtual_texture_info.GetSize();
        shadow_info.shadowmap_vt_page_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->VT_PAGE_SIZE;
        shadow_info.shadowmap_page_table_texture_size = virtual_texture_info.GetSize() / shadow_info.shadowmap_vt_page_size;
        GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<ShadowInfo>>()->UploadBuffer(resource_manager, &shadow_info, 0, sizeof(shadow_info));
    }
    
    return true;
}

bool glTFGraphicsPassMeshVTFeedback::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassVTFeedBackPS.hlsl)", RHIShaderType::Pixel, "main");

    GetGraphicsPipelineStateObject().BindRenderTargetFormats(
        {
            GetResourceDescriptor(GetVTFeedBackId(m_virtual_texture_id)).get(),
            &resource_manager.GetDepthDSV()
        });
    
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_DONT_CARE);
    
    if (m_shadowmap)
    {
        GetGraphicsPipelineStateObject().SetCullMode(RHICullMode::NONE);
        auto& shadow_macros = GetGraphicsPipelineStateObject().GetShaderMacros();
        shadow_macros.AddMacro("IS_SHADOWMAP_FEEDBACK", "1");    
    }
    
    return true;
}

bool glTFGraphicsPassMeshVTFeedback::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    GetResourceTexture(GetVTFeedBackId(m_virtual_texture_id))->Transition(resource_manager.GetCommandListForRecord(), RHIResourceStateType::STATE_RENDER_TARGET);
    
    std::vector render_targets
        {
            GetResourceDescriptor(GetVTFeedBackId(m_virtual_texture_id)).get(),
            &resource_manager.GetDepthDSV()
        };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;
    
    FeedbackConfig feedback_config;
    feedback_config.mipmap_offset = m_feedback_mipmap_offset;
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<FeedbackConfig>>()->UploadBuffer(resource_manager, &feedback_config, 0, sizeof(feedback_config));
    
    return true;
}

bool glTFGraphicsPassMeshVTFeedback::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))
    const auto& logical_texture_info = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetLogicalTextureInfo(m_virtual_texture_id);
    const auto& vt_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetVTFeedbackTextureSize(logical_texture_info);
    
    RHITextureDesc feed_back_desc = RHITextureDesc::MakeVirtualTextureFeedbackDesc(resource_manager, vt_size.first, vt_size.second);
    AddExportTextureResource(GetVTFeedBackId(m_virtual_texture_id), feed_back_desc, 
        {
            feed_back_desc.GetDataFormat(),
            RHIResourceDimension::TEXTURE2D,
            RHIViewType::RVT_RTV
        });
    
    return true;
}
