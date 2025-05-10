#include "glTFGraphicsPassMeshVSMPageRendering.h"

#include "glTFLight/glTFDirectionalLight.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

glTFGraphicsPassMeshVSMPageRendering::glTFGraphicsPassMeshVSMPageRendering(const VSMConfig& config)
    : m_config(config)
{
}

bool glTFGraphicsPassMeshVSMPageRendering::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
                                                                 const glTFSceneObjectBase& object)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::TryProcessSceneObject(resource_manager, object))

    const glTFDirectionalLight* light = dynamic_cast<const glTFDirectionalLight*>(&object);
    if (!light || light->GetID() != m_config.m_shadowmap_config.light_id)
    {
        return false;
    }

    m_directional_light = light;
    
    return true;
}

bool glTFGraphicsPassMeshVSMPageRendering::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshRVTPageRendering::InitPass(resource_manager))
    
    auto virtual_texture_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    GLTF_CHECK(virtual_texture_system);
    
    m_rvt_output_texture = virtual_texture_system->GetRVTPhysicalTexture()->GetTextureAllocation()->m_texture;

    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
            resource_manager.GetDevice(),
            m_rvt_output_texture,
            RHITextureDescriptorDesc(RHIDataFormat::D32_FLOAT, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_DSV),
            m_rvt_descriptor_allocations);


    resource_manager.GetRenderTargetManager().ClearRenderTarget(resource_manager.GetCommandListForRecord(), {m_rvt_descriptor_allocations.get()});
    
    return true;
}

bool glTFGraphicsPassMeshVSMPageRendering::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshRVTPageRendering::SetupPipelineStateObject(resource_manager))
    
    GetGraphicsPipelineStateObject().BindShaderCode(
                R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");

    auto virtual_texture_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    GLTF_CHECK(virtual_texture_system);
    
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(
            {
                m_rvt_descriptor_allocations.get()
            });
    GetGraphicsPipelineStateObject().SetCullMode(RHICullMode::NONE);
    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_WRITE);
    
    return true;
}

bool glTFGraphicsPassMeshVSMPageRendering::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshRVTPageRendering::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceVirtualShadowMapView>(m_config.m_shadowmap_config.light_id));
    
    return true;
}

bool glTFGraphicsPassMeshVSMPageRendering::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshRVTPageRendering::PreRenderPass(resource_manager))
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    m_rvt_output_texture->Transition(command_list, RHIResourceStateType::STATE_DEPTH_WRITE);
    std::vector render_targets
            {
                m_rvt_descriptor_allocations.get()
            };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = true;
    m_begin_rendering_info.clear_depth_stencil = true;
    
    // NDC y axis is opposite
    GetRenderInterface<glTFRenderInterfaceVirtualShadowMapView>()->SetNDCRange(m_page_rendering_info.page_x, 1.0f - m_page_rendering_info.mip_page_size - m_page_rendering_info.page_y,
                m_page_rendering_info.mip_page_size, m_page_rendering_info.mip_page_size);
    
    GetRenderInterface<glTFRenderInterfaceVirtualShadowMapView>()->CalculateShadowmapMatrix(resource_manager, *m_directional_light, resource_manager.GetSceneBounds());
    
    return true;
}
