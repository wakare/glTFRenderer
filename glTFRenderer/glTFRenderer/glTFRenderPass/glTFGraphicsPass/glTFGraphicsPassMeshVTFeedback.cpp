#include "glTFGraphicsPassMeshVTFeedback.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceVT.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

RHIViewportDesc glTFGraphicsPassMeshVTFeedback::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    const auto& vt_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetVTFeedbackTextureSize(resource_manager);
    return {0, 0, static_cast<float>(vt_size.first), static_cast<float>(vt_size.second), 0.0f, 1.0f};
}

bool glTFGraphicsPassMeshVTFeedback::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBaseSceneView::InitRenderInterface(resource_manager))
    
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceVT>(InterfaceVTType::RENDER_VT_FEEDBACK));
    
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
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_VT_Feedback).get(),
            &resource_manager.GetDepthDSV()
        });
    
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_DONT_CARE);
    
    return true;
}

bool glTFGraphicsPassMeshVTFeedback::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    GetResourceTexture(RenderPassResourceTableId::BasePass_VT_Feedback)->Transition(resource_manager.GetCommandListForRecord(), RHIResourceStateType::STATE_RENDER_TARGET);
    
    std::vector render_targets
        {
            GetResourceDescriptor(RenderPassResourceTableId::BasePass_VT_Feedback).get(),
            &resource_manager.GetDepthDSV()
        };

    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = GetGraphicsPipelineStateObject().GetDepthStencilMode() == RHIDepthStencilMode::DEPTH_WRITE;
    m_begin_rendering_info.clear_render_target = true;
    
    return true;
}

bool glTFGraphicsPassMeshVTFeedback::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitResourceTable(resource_manager))

    const auto& vt_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetVTFeedbackTextureSize(resource_manager);
    RHITextureDesc feed_back_desc = RHITextureDesc::MakeBasePassVTFeedbackDesc(resource_manager, vt_size.first, vt_size.second);
    AddExportTextureResource(RenderPassResourceTableId::BasePass_VT_Feedback, feed_back_desc, 
        {
            feed_back_desc.GetDataFormat(),
            RHIResourceDimension::TEXTURE2D,
            RHIViewType::RVT_RTV
        });

    AddFinalOutputCandidate(RenderPassResourceTableId::BasePass_VT_Feedback);
    
    return true;
}
