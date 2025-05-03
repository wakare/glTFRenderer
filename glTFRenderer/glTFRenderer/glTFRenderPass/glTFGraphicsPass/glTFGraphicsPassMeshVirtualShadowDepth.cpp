#include "glTFGraphicsPassMeshVirtualShadowDepth.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

struct VSMOutputTileOffset
{
    inline static std::string Name = "VSM_OUTPUT_FILE_OFFSET_REGISTER_INDEX";
    
    unsigned offset_x;
    unsigned offset_y;
    unsigned width;
    unsigned height;
};

glTFGraphicsPassMeshVirtualShadowDepth::glTFGraphicsPassMeshVirtualShadowDepth(const VSMConfig& config)
    : glTFGraphicsPassMeshBase()
    , m_config(config)
{
}

bool glTFGraphicsPassMeshVirtualShadowDepth::InitPass(glTFRenderResourceManager& resource_manager)
{
    auto virtual_texture_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    GLTF_CHECK(virtual_texture_system);
    m_rvt_output_texture = virtual_texture_system->GetRVTPhysicalTexture()->GetTextureAllocation()->m_texture;

    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
            resource_manager.GetDevice(),
            m_rvt_output_texture,
            RHITextureDescriptorDesc(RHIDataFormat::D32_FLOAT, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_DSV),
            m_rvt_descriptor_allocations);

    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))

    resource_manager.GetRenderTargetManager().ClearRenderTarget(resource_manager.GetCommandListForRecord(), {m_rvt_descriptor_allocations.get()});
    
    return true;
}

bool glTFGraphicsPassMeshVirtualShadowDepth::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupRootSignature(resource_manager))

    return true;
}

void glTFGraphicsPassMeshVirtualShadowDepth::SetupNextPageRenderingInfo(glTFRenderResourceManager& resource_manager, const VSMPageRenderingInfo& page_rendering_info)
{
    m_page_rendering_info = page_rendering_info;
    m_rendering_enabled = m_page_rendering_info.physical_page_x >= 0 && m_page_rendering_info.physical_page_y >= 0;
    if (m_rendering_enabled && m_directional_light)
    {
        // NDC y axis is opposite
        GetRenderInterface<glTFRenderInterfaceVirtualShadowMapView>()->SetNDCRange(page_rendering_info.page_x, 1.0f - page_rendering_info.mip_page_size - page_rendering_info.page_y,
            page_rendering_info.mip_page_size, page_rendering_info.mip_page_size);
        GetRenderInterface<glTFRenderInterfaceVirtualShadowMapView>()->CalculateShadowmapMatrix(resource_manager, *m_directional_light, resource_manager.GetSceneBounds());
    }
}

bool glTFGraphicsPassMeshVirtualShadowDepth::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
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

bool glTFGraphicsPassMeshVirtualShadowDepth::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<VSMOutputTileOffset>>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceVirtualShadowMapView>(m_config.m_shadowmap_config.light_id));
    
    return true;
}

bool glTFGraphicsPassMeshVirtualShadowDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))
    
    GetGraphicsPipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    /*
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\VSMOutputPS.hlsl)", RHIShaderType::Pixel, "main");
    */
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

bool glTFGraphicsPassMeshVirtualShadowDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    m_rvt_output_texture->Transition(command_list, RHIResourceStateType::STATE_DEPTH_WRITE);
    std::vector render_targets
            {
                m_rvt_descriptor_allocations.get()
            };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = true;
    m_begin_rendering_info.clear_depth_stencil = true;
    
    VSMOutputTileOffset offset;
    offset.offset_x = m_page_rendering_info.physical_page_x;
    offset.offset_y = m_page_rendering_info.physical_page_y;
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<VSMOutputTileOffset>>()->UploadBuffer(resource_manager, &offset, 0 ,sizeof(offset) );

    return true;
}

bool glTFGraphicsPassMeshVirtualShadowDepth::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PostRenderPass(resource_manager))

    m_rendering_enabled = false;
    
    return true;
}

RHIViewportDesc glTFGraphicsPassMeshVirtualShadowDepth::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    RHIViewportDesc viewport_desc
        {
            static_cast<float>(m_page_rendering_info.physical_page_x), static_cast<float>(m_page_rendering_info.physical_page_y),
            static_cast<float>(m_page_rendering_info.physical_page_size), static_cast<float>(m_page_rendering_info.physical_page_size),
            0.0f, 1.0f
        };

    return viewport_desc;
}
