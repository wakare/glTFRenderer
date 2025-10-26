#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemSceneRenderer.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererModule/RendererModuleSceneMesh.h"

class RendererSystemLighting : public RendererSystemBase
{
public:
    RendererSystemLighting(RendererInterface::ResourceOperator& resource_operator, std::shared_ptr<RendererSystemSceneRenderer> scene);
    
    bool AddLight(const LightInfo& light_info);
    
    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;

protected:
    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererModuleLighting> m_lighting_module;

    RendererInterface::RenderGraphNodeHandle m_lighting_pass_node{NULL_HANDLE};
};
