#pragma once
#include "glTFRenderSystem/RenderSystemBase.h"

class ShadowRenderSystem : public RenderSystemBase
{
public:
    enum
    {
        SHADOWMAP_SIZE = 2048,
    };
    virtual bool InitRenderSystem(glTFRenderResourceManager& resource_manager) override;
    virtual void SetupPass(glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph) override;
    virtual void ShutdownRenderSystem() override;
    virtual void TickRenderSystem(glTFRenderResourceManager& resource_manager) override;
};
