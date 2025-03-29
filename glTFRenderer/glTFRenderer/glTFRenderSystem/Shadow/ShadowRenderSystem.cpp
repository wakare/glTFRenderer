#include "ShadowRenderSystem.h"

#include "glTFLight/glTFDirectionalLight.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshShadowDepth.h"

bool ShadowRenderSystem::InitRenderSystem(glTFRenderResourceManager& resource_manager)
{
    return true;   
}

void ShadowRenderSystem::SetupPass(glTFRenderPassManager& pass_manager,  const glTFSceneGraph& scene_graph)
{
    auto directional_lights = scene_graph.GetAllTypedNodes<glTFDirectionalLight>();
    for (const auto& directional_light : directional_lights)
    {
        ShadowmapPassConfig config{};
        config.light_id = static_cast<int>(directional_light->GetID());
        pass_manager.AddRenderPass(SCENE_SHADOW, std::make_shared<glTFGraphicsPassMeshShadowDepth>(config));
    }
}

void ShadowRenderSystem::ShutdownRenderSystem()
{
}

void ShadowRenderSystem::TickRenderSystem(glTFRenderResourceManager& resource_manager)
{
}
