#include "ShadowRenderSystem.h"

#include "glTFLight/glTFDirectionalLight.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshShadowDepth.h"
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
        else
        {
            m_virtual_texture_page_size = vt_system->VT_PAGE_SIZE;
        }
    }
    
    return true;   
}

void ShadowRenderSystem::SetupPass(glTFRenderPassManager& pass_manager,  const glTFSceneGraph& scene_graph)
{
    auto directional_lights = scene_graph.GetAllTypedNodes<glTFDirectionalLight>();
    for (const auto& directional_light : directional_lights)
    {
        if (m_virtual_shadow_map)
        {
            
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

    GLTF_CHECK(VIRTUAL_SHADOWMAP_SIZE % page_size == 0);
    
    width = VIRTUAL_SHADOWMAP_SIZE / page_size;
    height = width;
    
    return true;
}
