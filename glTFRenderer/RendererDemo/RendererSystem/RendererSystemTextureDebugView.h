#pragma once

#include "RendererSystemBase.h"
#include "RenderPassSetupBuilder.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class RendererSystemToneMap;

class RendererSystemTextureDebugView : public RendererSystemBase
{
public:
    enum class VisualizationMode : unsigned
    {
        Color = 0,
        Scalar = 1,
        Velocity = 2
    };

    struct SourceResolution
    {
        RendererInterface::RenderTargetHandle render_target{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle dependency_node{NULL_HANDLE};
    };

    using SourceResolver = std::function<SourceResolution()>;

    struct DebugSourceDesc
    {
        std::string id{};
        std::string display_name{};
        VisualizationMode visualization{VisualizationMode::Color};
        float default_scale{1.0f};
        float default_bias{0.0f};
        bool default_apply_tonemap{false};
        SourceResolver resolver{};
    };

    struct DebugState
    {
        std::string source_id{};
        float scale{1.0f};
        float bias{0.0f};
        bool apply_tonemap{false};
    };

    explicit RendererSystemTextureDebugView(std::shared_ptr<RendererSystemToneMap> tone_map);

    bool RegisterSource(DebugSourceDesc source_desc);
    bool SelectSourceById(const std::string& source_id, bool reset_to_defaults = true);
    const DebugState& GetDebugState() const { return m_debug_state; }
    bool SetDebugState(const DebugState& state);

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Texture Debug View"; }
    virtual void DrawDebugUI() override;

protected:
    struct PassSetupCache
    {
        bool valid{false};
        RendererInterface::RenderTargetHandle source_render_target{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle dependency_node{NULL_HANDLE};

        void Reset()
        {
            valid = false;
            source_render_target = NULL_HANDLE;
            dependency_node = NULL_HANDLE;
        }
    };

    struct ExecutionPlan
    {
        const DebugSourceDesc* source{nullptr};
        SourceResolution resolved_source{};
        RenderFeature::ComputeExecutionPlan compute_plan{};
    };

    struct TextureDebugViewGlobalParams
    {
        float exposure{1.0f};
        float gamma{2.2f};
        unsigned tone_map_mode{1u};
        unsigned visualization_mode{0u};
        float scale{1.0f};
        float bias{0.0f};
        unsigned apply_tonemap{0u};
        float pad0{0.0f};
    };

    static bool IsScalarVisualization(VisualizationMode visualization);
    static void ClampDebugState(DebugState& state);
    int FindSourceIndexById(const std::string& source_id) const;
    bool ApplySourceDefaults(size_t source_index);
    const DebugSourceDesc* GetSelectedSource() const;
    ExecutionPlan BuildExecutionPlan(RendererInterface::ResourceOperator& resource_operator) const;
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildColorPassSetupInfo(const ExecutionPlan& execution_plan) const;
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildScalarPassSetupInfo(const ExecutionPlan& execution_plan) const;
    bool SyncActivePassNode(RendererInterface::ResourceOperator& resource_operator,
                            RendererInterface::RenderGraph& graph,
                            const ExecutionPlan& execution_plan,
                            RendererInterface::RenderGraphNodeHandle& out_active_node_handle);
    void UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator, VisualizationMode visualization);

    std::shared_ptr<RendererSystemToneMap> m_tone_map;
    std::vector<DebugSourceDesc> m_sources{};

    RendererInterface::RenderGraphNodeHandle m_color_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_scalar_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_output{NULL_HANDLE};
    RendererInterface::BufferHandle m_global_params_handle{NULL_HANDLE};
    PassSetupCache m_color_pass_cache{};
    PassSetupCache m_scalar_pass_cache{};

    DebugState m_debug_state{};
};
