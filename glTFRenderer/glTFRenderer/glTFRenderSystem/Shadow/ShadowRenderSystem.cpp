#include "ShadowRenderSystem.h"

#include "glTFLight/glTFDirectionalLight.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshShadowDepth.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassVirtualShadowmapFeedback.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"

bool ShadowRenderSystem::InitRenderSystem(glTFRenderResourceManager& resource_manager)
{
    if (m_virtual_shadow_map)
    {
        auto vt_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
        if (!vt_system)
        {
            m_virtual_shadow_map = false;
            return false;
        }
        
        m_virtual_texture_page_size = vt_system->VT_PAGE_SIZE;
    }
    
    return true;   
}

void ShadowRenderSystem::SetupPass(glTFRenderResourceManager& resource_manager, glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph)
{
    auto directional_lights = scene_graph.GetAllTypedNodes<glTFDirectionalLight>();
    auto vt_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    auto shadow_system = resource_manager.GetRenderSystem<ShadowRenderSystem>();
    
    for (const auto& directional_light : directional_lights)
    {
        if (m_virtual_shadow_map)
        {
            GLTF_CHECK(vt_system);
            
            auto virtual_shadow_map_texture = std::make_shared<VTShadowmapLogicalTexture>(directional_light->GetID());
            RHITextureDesc shadowmap_logical_texture_desc
            {
                "Shadowmap logical texture",
                static_cast<unsigned>(shadow_system->VIRTUAL_SHADOWMAP_SIZE),
                static_cast<unsigned>(shadow_system->VIRTUAL_SHADOWMAP_SIZE),
                RHIDataFormat::R32_TYPELESS,
                static_cast<RHIResourceUsageFlags>(RUF_ALLOW_DEPTH_STENCIL | RUF_ALLOW_SRV),
    {
                    .clear_format = RHIDataFormat::D32_FLOAT,
                    .clear_depth_stencil{1.0f, 0}
                },
            };
            
            VTLogicalTextureConfig config{};
            config.virtual_texture_id = vt_system->GetAvailableVTIdAndInc();
            config.isSVT = false;
            virtual_shadow_map_texture->InitLogicalTexture(shadowmap_logical_texture_desc, config);
            vt_system->RegisterTexture(virtual_shadow_map_texture);
            
            m_virtual_shadow_map_textures.push_back(virtual_shadow_map_texture);
        }
        else
        {
            ShadowmapPassConfig config{};
            config.light_id = static_cast<int>(directional_light->GetID());
            config.shadowmap_width = SHADOWMAP_SIZE;
            config.shadowmap_height = SHADOWMAP_SIZE;
            pass_manager.AddRenderPass(SCENE_SHADOW, std::make_shared<glTFGraphicsPassMeshShadowDepth>(config));    
        }
    }
}

void ShadowRenderSystem::ShutdownRenderSystem()
{
}

void ShadowRenderSystem::TickRenderSystem(glTFRenderResourceManager& resource_manager)
{
}

bool ShadowRenderSystem::GetVirtualShadowmapFeedbackSize(const glTFRenderResourceManager& resource_manager, int& width, int& height)
{
    auto vt_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    auto page_size = vt_system->VT_PAGE_SIZE;

    GLTF_CHECK(static_cast<int>(VIRTUAL_SHADOWMAP_SIZE) % page_size == 0);
    
    width = static_cast<int>(VIRTUAL_SHADOWMAP_SIZE) / static_cast<int>(page_size);
    height = width;
    
    return true;
}

bool ShadowRenderSystem::IsVSM() const
{
    return m_virtual_shadow_map;
}
