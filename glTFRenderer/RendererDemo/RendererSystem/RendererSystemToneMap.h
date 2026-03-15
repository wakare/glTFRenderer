#pragma once
#include "RendererSystemBase.h"
#include "RenderPassSetupBuilder.h"
#include <memory>
#include <utility>

class RendererSystemFrostedGlass;
class RendererSystemLighting;
class RendererSystemSceneRenderer;
class RendererSystemSSAO;

class RendererSystemToneMap : public RendererSystemBase
{
public:
    RendererSystemToneMap(std::shared_ptr<RendererSystemFrostedGlass> frosted,
                          std::shared_ptr<RendererSystemLighting> lighting,
                          std::shared_ptr<RendererSystemSceneRenderer> scene,
                          std::shared_ptr<RendererSystemSSAO> ssao);

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Tone Map"; }
    virtual void DrawDebugUI() override;

protected:
    struct ToneMapExecutionPlan
    {
        RendererInterface::RenderTargetHandle input_color{NULL_HANDLE};
        RenderFeature::ComputeExecutionPlan compute_plan{};
    };

    struct ToneMapGlobalParams
    {
        float exposure{1.0f};
        float gamma{2.2f};
        unsigned tone_map_mode{1}; // 0: Reinhard, 1: ACES
        unsigned debug_view_mode{0}; // 0: Final, 1: Velocity, 2: SSAO
        float debug_velocity_scale{32.0f};
        float pad0{0.0f};
        float pad1{0.0f};
        float pad2{0.0f};
    };

    RendererInterface::RenderGraph::RenderPassSetupInfo BuildToneMapPassSetupInfo(
        const ToneMapExecutionPlan& execution_plan) const;
    ToneMapExecutionPlan BuildToneMapExecutionPlan(RendererInterface::ResourceOperator& resource_operator) const;
    void UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator);

    std::shared_ptr<RendererSystemFrostedGlass> m_frosted;
    std::shared_ptr<RendererSystemLighting> m_lighting;
    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemSSAO> m_ssao;

    RendererInterface::RenderGraphNodeHandle m_tone_map_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_tone_map_output{NULL_HANDLE};
    RendererInterface::BufferHandle m_tone_map_global_params_handle{NULL_HANDLE};

    ToneMapGlobalParams m_global_params{};
    bool m_need_upload_params{true};
};
