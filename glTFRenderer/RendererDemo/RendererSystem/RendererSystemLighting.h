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
    
    bool AddLight(const LightInfo& light_info);
    
    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;

protected:
    struct ShadowPassResource
    {
        static ViewBuffer CalcDirectionalLightShadowMatrix(const LightInfo& directional_light_info, const RendererSceneAABB& scene_bounds, float ndc_min_x, float ndc_min_y, float ndc_width, float ndc_height);
        
        RendererInterface::RenderGraphNodeHandle m_shadow_pass_node {NULL_HANDLE};
        RendererInterface::RenderTargetHandle m_shadow_map {NULL_HANDLE};
        ViewBuffer m_shadow_map_view_buffer{};
        RendererInterface::BufferHandle m_shadow_map_buffer_handle{NULL_HANDLE};
    };
    
    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererModuleLighting> m_lighting_module;

    std::map<unsigned, ShadowPassResource> m_shadow_pass_resources;
    RendererInterface::RenderGraphNodeHandle m_lighting_pass_node {NULL_HANDLE};
};
