#pragma once
#include "glTFRenderSystem/RenderSystemBase.h"

class VTLogicalTextureBase;

class ShadowRenderSystem : public RenderSystemBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(ShadowRenderSystem)
    
    enum
    {
        SHADOWMAP_SIZE = 2048,
        VIRTUAL_SHADOWMAP_SIZE = 8192,
    };
    
    virtual bool InitRenderSystem(glTFRenderResourceManager& resource_manager) override;
    virtual void SetupPass(glTFRenderResourceManager& resource_manager, glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph) override;
    virtual void ShutdownRenderSystem() override;
    virtual void TickRenderSystem(glTFRenderResourceManager& resource_manager) override;

    static bool GetVirtualShadowmapFeedbackSize(const glTFRenderResourceManager& resource_manager, int& width, int& height);
    bool IsVSM() const;

    const std::vector<std::shared_ptr<VTLogicalTextureBase>>& GetVSM() const;
    
protected:
    bool m_virtual_shadow_map {true};
    unsigned m_virtual_texture_page_size {0};

    std::vector<std::shared_ptr<VTLogicalTextureBase>> m_virtual_shadow_map_textures;
};
