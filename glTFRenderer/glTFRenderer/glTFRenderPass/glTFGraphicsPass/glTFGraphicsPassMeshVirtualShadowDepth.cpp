#include "glTFGraphicsPassMeshVirtualShadowDepth.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderSystem/Shadow/ShadowRenderSystem.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

struct VSMOutputTileOffset
{
    inline static std::string Name = "VSM_OUTPUT_FILE_OFFSET_REGISTER_INDEX";
    
    unsigned offset_x;
    unsigned offset_y;
    unsigned width;
    unsigned height;
};

glTFGraphicsPassMeshVirtualShadowDepth::glTFGraphicsPassMeshVirtualShadowDepth(const VSMConfig& config)
    : glTFGraphicsPassMeshShadowDepth(config.m_shadowmap_config)
    , m_config(config)
{
}

bool glTFGraphicsPassMeshVirtualShadowDepth::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshShadowDepth::InitPass(resource_manager))
    
    auto virtual_texture_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    GLTF_CHECK(virtual_texture_system);
    m_rvt_output_texture = virtual_texture_system->GetRVTPhysicalTexture()->GetTextureAllocation()->m_texture;

    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
            resource_manager.GetDevice(),
            m_rvt_output_texture,
            RHITextureDescriptorDesc(m_rvt_output_texture->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_RTV),
            m_rvt_descriptor_allocations);
    
    return true;
}

bool glTFGraphicsPassMeshVirtualShadowDepth::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshShadowDepth::SetupRootSignature(resource_manager))

    return true;
}

void glTFGraphicsPassMeshVirtualShadowDepth::UpdateNextRenderPageTileOffset(int x, int y, unsigned tile_size)
{
    m_next_render_page_tile_offset_x = x;
    m_next_render_page_tile_offset_y = y;
    m_tile_size = tile_size;
    
    m_rendering_enabled = m_next_render_page_tile_offset_x >= 0 && m_next_render_page_tile_offset_y >= 0;
}

bool glTFGraphicsPassMeshVirtualShadowDepth::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshVirtualShadowDepth::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<VSMOutputTileOffset>>());
    
    return true;
}

bool glTFGraphicsPassMeshVirtualShadowDepth::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshVirtualShadowDepth::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\VSMOutputPS.hlsl)", RHIShaderType::Pixel, "main");

    auto virtual_texture_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    GLTF_CHECK(virtual_texture_system);

    GetGraphicsPipelineStateObject().SetDepthStencilState(RHIDepthStencilMode::DEPTH_DONT_CARE);
    
    return true;}

bool glTFGraphicsPassMeshVirtualShadowDepth::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    m_rvt_output_texture->Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    std::vector render_targets
            {
                m_rvt_descriptor_allocations.get()
            };
    
    m_begin_rendering_info.m_render_targets = render_targets;
    m_begin_rendering_info.enable_depth_write = true;
    m_begin_rendering_info.clear_render_target = false;
    
    VSMOutputTileOffset offset;
    offset.offset_x = m_next_render_page_tile_offset_x;
    offset.offset_y = m_next_render_page_tile_offset_y;
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<VSMOutputTileOffset>>()->UploadBuffer(resource_manager, &offset, 0 ,sizeof(offset) );
    
    return true;
}

bool glTFGraphicsPassMeshVirtualShadowDepth::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshShadowDepth::PostRenderPass(resource_manager))

    m_rendering_enabled = false;
    
    return true;
}

RHIViewportDesc glTFGraphicsPassMeshVirtualShadowDepth::GetViewport(glTFRenderResourceManager& resource_manager) const
{
    RHIViewportDesc viewport_desc
        {
            static_cast<float>(m_next_render_page_tile_offset_x), static_cast<float>(m_next_render_page_tile_offset_y),
            static_cast<float>(m_tile_size), static_cast<float>(m_tile_size),
            0.0f, 1.0f
        };

    return viewport_desc;
}
