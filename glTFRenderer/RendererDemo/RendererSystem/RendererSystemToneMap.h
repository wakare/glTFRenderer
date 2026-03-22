#pragma once
#include "RendererSystemBase.h"
#include "RenderPassSetupBuilder.h"
#include <memory>
#include <utility>

class RendererSystemFrostedGlass;
class RendererSystemLighting;

class RendererSystemToneMap : public RendererSystemBase
{
public:
    struct ToneMapGlobalParams
    {
        float exposure{1.0f};
        float gamma{2.2f};
        unsigned tone_map_mode{1}; // 0: Reinhard, 1: ACES
        float pad0{0.0f};
    };

    RendererSystemToneMap(std::shared_ptr<RendererSystemFrostedGlass> frosted,
                          std::shared_ptr<RendererSystemLighting> lighting);

    RendererInterface::RenderTargetHandle GetOutput() const { return m_tone_map_output; }
    RendererInterface::RenderGraphNodeHandle GetOutputNode() const { return m_tone_map_pass_node; }
    const ToneMapGlobalParams& GetGlobalParams() const { return m_global_params; }
    void SetGlobalParams(const ToneMapGlobalParams& global_params);

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

    RendererInterface::RenderGraph::RenderPassSetupInfo BuildToneMapPassSetupInfo(
        const ToneMapExecutionPlan& execution_plan) const;
    ToneMapExecutionPlan BuildToneMapExecutionPlan(RendererInterface::ResourceOperator& resource_operator) const;
    static void ClampGlobalParams(ToneMapGlobalParams& global_params);
    void UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator);

    std::shared_ptr<RendererSystemFrostedGlass> m_frosted;
    std::shared_ptr<RendererSystemLighting> m_lighting;

    RendererInterface::RenderGraphNodeHandle m_tone_map_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_tone_map_output{NULL_HANDLE};
    RendererInterface::BufferHandle m_tone_map_global_params_handle{NULL_HANDLE};

    ToneMapGlobalParams m_global_params{};
    bool m_need_upload_params{true};
};
