#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemSceneRenderer.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererModule/RendererModuleSceneMesh.h"

class RendererSceneAABB;

class RendererSystemLighting : public RendererSystemBase
{
public:
    RendererSystemLighting(RendererInterface::ResourceOperator& resource_operator, std::shared_ptr<RendererSystemSceneRenderer> scene);
    
    unsigned AddLight(const LightInfo& light_info);
    bool UpdateLight(unsigned index, const LightInfo& light_info);
    bool GetDominantDirectionalLight(glm::fvec3& out_direction, float& out_luminance) const;
    bool CastShadow() const;
    void SetCastShadow(bool cast_shadow);
    RendererInterface::RenderTargetHandle GetLightingOutput() const;
    
    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Lighting"; }
    virtual void DrawDebugUI() override;
    
protected:
    struct ShadowMapInfo
    {
        glm::fmat4x4 view_matrix{1.0f};
        glm::fmat4x4 projection_matrix{1.0f};
        unsigned shadowmap_size[2];
        unsigned vsm_texture_id;
        unsigned pad;    
    };
    
    struct ShadowPassResource
    {
        static bool CalcDirectionalLightShadowMatrix(const LightInfo& directional_light_info, const RendererSceneAABB& scene_bounds, float ndc_min_x, float ndc_min_y, float ndc_width, float ndc_height, unsigned
                                                           shadowmap_width, unsigned shadowmap_height, ViewBuffer& out_view_buffer, ShadowMapInfo& out_shadow_info);
        
        RendererInterface::RenderGraphNodeHandle m_shadow_pass_node {NULL_HANDLE};
        RendererInterface::RenderTargetHandle m_shadow_map {NULL_HANDLE};
        ViewBuffer m_shadow_map_view_buffer{};
        ShadowMapInfo m_shadow_map_info{};
        RendererInterface::BufferHandle m_shadow_map_buffer_handle{NULL_HANDLE};
    };

    void UpdateDirectionalShadowResources(RendererInterface::ResourceOperator& resource_operator);

    bool m_cast_shadow {true};
    
    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererModuleLighting> m_lighting_module;

    std::map<unsigned, ShadowPassResource> m_shadow_pass_resources;
    RendererInterface::RenderGraphNodeHandle m_lighting_pass_node {NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_lighting_pass_output {NULL_HANDLE};

    RendererInterface::BufferHandle m_lighting_pass_shadow_infos_handle {NULL_HANDLE};
};
