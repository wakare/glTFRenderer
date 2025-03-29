#include "glTFGraphicsPassMeshShadowDepth.h"

#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceShadowmap.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

glTFGraphicsPassMeshShadowDepth::glTFGraphicsPassMeshShadowDepth(const ShadowmapPassConfig& config)
    : m_config(config)
{
    
}

bool glTFGraphicsPassMeshShadowDepth::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceShadowMapView>(m_config.light_id));
    
    return true;
}

bool glTFGraphicsPassMeshShadowDepth::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitResourceTable(resource_manager));

    auto shadowmap_desc = RHITextureDesc::MakeShadowPassOutputDesc(resource_manager); 
    AddExportTextureResource(RenderPassResourceTableId::ShadowPass_Output, shadowmap_desc, 
    {
        RHIDataFormat::D32_FLOAT,
        RHIResourceDimension::TEXTURE2D,
        RHIViewType::RVT_DSV
    });

    AddFinalOutputCandidate(RenderPassResourceTableId::ShadowPass_Output);
    
    return true;
}

bool glTFGraphicsPassMeshShadowDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Transition swapchain state to render target for shading
    GetResourceTexture(RenderPassResourceTableId::ShadowPass_Output)->Transition(command_list, RHIResourceStateType::STATE_DEPTH_WRITE);

    std::vector render_targets
        {
            GetResourceDescriptor(RenderPassResourceTableId::ShadowPass_Output).get()
        };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = true;
    m_begin_rendering_info.clear_render_target = true;
    
    return true;
}

bool glTFGraphicsPassMeshShadowDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))
    
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    GetResourceTexture(RenderPassResourceTableId::ShadowPass_Output)->Transition(command_list, RHIResourceStateType::STATE_DEPTH_WRITE);
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);

    GetGraphicsPipelineStateObject().BindRenderTargetFormats(
        {
            GetResourceDescriptor(RenderPassResourceTableId::ShadowPass_Output).get()
        });
    GetGraphicsPipelineStateObject().SetCullMode(RHICullMode::NONE);
    
    
    return true;
}

RHIViewportDesc glTFGraphicsPassMeshShadowDepth::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    return {0.0f, 0.0f, static_cast<float>(m_config.shadowmap_width), static_cast<float>(m_config.shadowmap_height), 0.0f, 1.0f};
}
