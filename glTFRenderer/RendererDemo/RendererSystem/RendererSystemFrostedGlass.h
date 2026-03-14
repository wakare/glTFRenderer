#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemLighting.h"
#include "RendererSystemSceneRenderer.h"
#include "PostFxSharedResources.h"
#include <array>
#include <cstddef>
#include <functional>
#include <glm/glm/glm.hpp>
#include <utility>
#include <vector>

class RendererSystemFrostedGlass : public RendererSystemBase
{
public:
    enum class PanelShapeType : unsigned
    {
        RoundedRect = 0,
        Circle = 1,
        ShapeMask = 2
    };

    enum class PanelInteractionState : unsigned
    {
        Idle = 0,
        Hover = 1,
        Grab = 2,
        Move = 3,
        Scale = 4,
        Count = 5
    };

    enum class PanelPayloadPath : unsigned
    {
        ComputeSDF = 0,
        RasterPanelGBuffer = 1
    };

    enum class PanelDepthPolicy : unsigned
    {
        Overlay = 0,
        SceneOcclusion = 1
    };

    enum class BlurSourceMode : unsigned
    {
        LegacyPyramid = 0,
        SharedMip = 1,
        SharedDual = 2
    };

    static constexpr unsigned PANEL_INTERACTION_STATE_COUNT = static_cast<unsigned>(PanelInteractionState::Count);

    struct PanelStateCurve
    {
        float blur_sigma_scale{1.0f};
        float blur_strength_scale{1.0f};
        float rim_intensity_scale{1.0f};
        float fresnel_intensity_scale{1.0f};
        float alpha_scale{1.0f};
    };

    struct FrostedGlassPanelDesc
    {
        glm::fvec2 center_uv{0.5f, 0.52f};
        glm::fvec2 half_size_uv{0.30f, 0.20f};
        unsigned world_space_mode{0}; // 0: screen-space panel, 1: world-space panel
        PanelDepthPolicy depth_policy{PanelDepthPolicy::Overlay};
        float world_space_pad0{0.0f};
        glm::fvec3 world_center{0.0f, 1.25f, -0.75f};
        float world_space_pad1{0.0f};
        glm::fvec3 world_axis_u{0.70f, 0.00f, 0.00f}; // half-extent world vector
        float world_space_pad2{0.0f};
        glm::fvec3 world_axis_v{0.00f, 0.45f, 0.00f}; // half-extent world vector
        float world_space_pad3{0.0f};
        float corner_radius{0.03f};
        float blur_sigma{8.5f};
        float blur_strength{0.92f};
        float rim_intensity{0.03f};
        glm::fvec3 tint_color{0.94f, 0.97f, 1.0f};
        PanelShapeType shape_type{PanelShapeType::RoundedRect};
        float edge_softness{1.25f};
        float thickness{0.014f};
        float refraction_strength{0.90f};
        float fresnel_intensity{0.02f};
        float fresnel_power{6.0f};
        float custom_shape_index{0.0f};
        float panel_alpha{1.0f};
        float layer_order{0.0f};
        PanelInteractionState interaction_state{PanelInteractionState::Idle};
        float interaction_transition_speed{10.0f};
        float interaction_pad0{0.0f};
        float interaction_pad1{0.0f};
        std::array<PanelStateCurve, PANEL_INTERACTION_STATE_COUNT> state_curves{{
            {1.00f, 1.00f, 1.00f, 1.00f, 1.00f}, // Idle
            {1.12f, 1.05f, 1.05f, 1.03f, 1.00f}, // Hover
            {0.98f, 0.95f, 1.12f, 1.08f, 0.96f}, // Grab
            {1.22f, 1.12f, 1.02f, 0.98f, 0.92f}, // Move
            {1.28f, 1.16f, 1.06f, 1.00f, 0.92f}  // Scale
        }};
    };

    enum
    {
        MAX_PANEL_COUNT = 16
    };

    using ExternalPanelProducer = std::function<void(
        std::vector<FrostedGlassPanelDesc>& out_world_space_panels,
        std::vector<FrostedGlassPanelDesc>& out_overlay_panels)>;

    RendererSystemFrostedGlass(std::shared_ptr<RendererSystemSceneRenderer> scene,
                               std::shared_ptr<RendererSystemLighting> lighting);

    unsigned AddPanel(const FrostedGlassPanelDesc& panel_desc);
    bool UpdatePanel(unsigned index, const FrostedGlassPanelDesc& panel_desc);
    unsigned RegisterExternalPanelProducer(ExternalPanelProducer producer);
    bool UnregisterExternalPanelProducer(unsigned producer_id);
    void ClearExternalPanelProducers();
    void SetExternalWorldSpacePanels(const std::vector<FrostedGlassPanelDesc>& panel_descs);
    void SetExternalOverlayPanels(const std::vector<FrostedGlassPanelDesc>& panel_descs);
    void ClearExternalPanels();
    bool ContainsPanel(unsigned index) const;
    bool SetPanelInteractionState(unsigned index, PanelInteractionState interaction_state);
    PanelInteractionState GetPanelInteractionState(unsigned index) const;
    void SetBlurSourceMode(BlurSourceMode mode);
    BlurSourceMode GetBlurSourceMode() const { return m_blur_source_mode; }
    void SetFullFogMode(bool enable);
    bool IsFullFogModeEnabled() const { return true; }
    void ForceResetTemporalHistory();
    unsigned GetEffectivePanelCount() const { return m_global_params.panel_count; }
    RendererInterface::RenderTargetHandle GetOutput() const { return m_composite_outputs.final_output; }
    RendererInterface::RenderTargetHandle GetHalfResPing() const
    {
        return m_postfx_shared_resources.GetPing(PostFxSharedResources::Resolution::Half);
    }
    RendererInterface::RenderTargetHandle GetHalfResPong() const
    {
        return m_postfx_shared_resources.GetPong(PostFxSharedResources::Resolution::Half);
    }
    RendererInterface::RenderTargetHandle GetQuarterResPing() const
    {
        return m_postfx_shared_resources.GetPing(PostFxSharedResources::Resolution::Quarter);
    }
    RendererInterface::RenderTargetHandle GetQuarterResPong() const
    {
        return m_postfx_shared_resources.GetPong(PostFxSharedResources::Resolution::Quarter);
    }

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Frosted Glass"; }
    virtual void DrawDebugUI() override;

protected:
    struct FrostedGlassPanelGpuData
    {
        glm::fvec4 center_half_size{};
        glm::fvec4 world_center_mode{};
        glm::fvec4 world_axis_u{};
        glm::fvec4 world_axis_v{};
        glm::fvec4 corner_blur_rim{};
        glm::fvec4 tint_depth_weight{};
        glm::fvec4 shape_info{};
        glm::fvec4 optical_info{};
        glm::fvec4 layering_info{};
    };

    struct FrostedGlassGlobalParams
    {
        unsigned panel_count{0};
        unsigned blur_radius{9};
        float scene_edge_scale{40.0f};
        float blur_kernel_sigma_scale{1.90f};
        float temporal_history_blend{0.90f};
        float temporal_reject_velocity{0.03f};
        float temporal_edge_reject{1.0f};
        unsigned temporal_history_valid{0};
        unsigned blur_source_mode{1};
        unsigned multilayer_mode{1};
        float multilayer_overlap_threshold{0.08f};
        float frosted_padding5{0.0f};
        float frosted_padding6{0.0f};
        unsigned multilayer_runtime_enabled{1};
        float multilayer_frame_budget_ms{22.0f};
        float frosted_padding2{0.0f};
        float blur_response_scale{2.20f};
        float blur_sigma_normalization{5.5f};
        float blur_veil_strength{1.35f};
        float frosted_padding0{0.0f};
        float frosted_padding1{0.0f};
        unsigned nan_debug_mode{0};
        unsigned frosted_flags0{0};
        float thickness_edge_power{2.20f};
        float thickness_highlight_boost_max{2.60f};
        float thickness_refraction_boost_max{1.90f};
        float thickness_edge_shadow_strength{0.20f};
        float thickness_range_min{0.004f};
        float thickness_range_max{0.060f};
        float edge_spec_intensity{0.95f};
        float frosted_padding3{0.0f};
        float edge_highlight_width{0.34f};
        float edge_highlight_white_mix{0.88f};
        float directional_highlight_min{0.10f};
        float directional_highlight_max{2.20f};
        float directional_highlight_curve{1.50f};
        glm::fvec4 highlight_light_dir_weight{0.0f, -1.0f, 0.0f, 0.0f}; // xyz: dominant directional light dir, w: valid flag
    };
    static_assert(sizeof(FrostedGlassGlobalParams) == 160, "FrostedGlassGlobalParams must match HLSL cbuffer size.");
    static_assert(offsetof(FrostedGlassGlobalParams, highlight_light_dir_weight) == 144,
                  "FrostedGlassGlobalParams::highlight_light_dir_weight offset mismatch with HLSL cbuffer.");

    struct PanelRuntimeState
    {
        PanelInteractionState target_state{PanelInteractionState::Idle};
        std::array<float, PANEL_INTERACTION_STATE_COUNT> state_weights{{1.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
    };

    struct ExternalPanelProducerEntry
    {
        unsigned producer_id{0};
        ExternalPanelProducer producer{};
    };

    enum class DebugPanelSource : unsigned
    {
        Internal = 0,
        ProducerWorld = 1,
        ProducerOverlay = 2,
        ManualWorld = 3,
        ManualOverlay = 4
    };

    struct DebugPanelOverrideBucket
    {
        std::vector<FrostedGlassPanelDesc> panel_descs{};
        std::vector<unsigned char> enabled{};
    };

    static constexpr unsigned BLUR_LEVEL_COUNT = 5u;
    using BlurDispatchSizes = std::array<std::pair<unsigned, unsigned>, BLUR_LEVEL_COUNT>;

    struct TickDispatchDimensions
    {
        unsigned full_dispatch_x{0};
        unsigned full_dispatch_y{0};
        BlurDispatchSizes blur_dispatch_sizes{};
    };

    struct BlurPyramidLevelView
    {
        RendererInterface::RenderGraphNodeHandle downsample_node{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle blur_horizontal_node{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle blur_vertical_node{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle shared_downsample_node{NULL_HANDLE};
        RendererInterface::RenderTargetHandle ping{NULL_HANDLE};
        RendererInterface::RenderTargetHandle pong{NULL_HANDLE};
        RendererInterface::RenderTargetHandle output{NULL_HANDLE};

        bool HasLegacyPath() const
        {
            return downsample_node != NULL_HANDLE &&
                   blur_horizontal_node != NULL_HANDLE &&
                   blur_vertical_node != NULL_HANDLE &&
                   ping != NULL_HANDLE &&
                   pong != NULL_HANDLE &&
                   output != NULL_HANDLE;
        }

        bool HasSharedMipPath() const
        {
            return shared_downsample_node != NULL_HANDLE && output != NULL_HANDLE;
        }
    };

    struct BlurPyramidBundleView
    {
        std::array<BlurPyramidLevelView, BLUR_LEVEL_COUNT> levels{};

        const BlurPyramidLevelView& Half() const { return levels[0]; }
        const BlurPyramidLevelView& Quarter() const { return levels[1]; }
        const BlurPyramidLevelView& Eighth() const { return levels[2]; }
        const BlurPyramidLevelView& Sixteenth() const { return levels[3]; }
        const BlurPyramidLevelView& ThirtySecond() const { return levels[4]; }

        bool HasLegacyPath() const
        {
            for (const auto& level : levels)
            {
                if (!level.HasLegacyPath())
                {
                    return false;
                }
            }
            return true;
        }

        bool HasSharedMipPath() const
        {
            for (const auto& level : levels)
            {
                if (!level.HasSharedMipPath())
                {
                    return false;
                }
            }
            return true;
        }
    };

    struct BlurPyramidStorage
    {
        std::array<BlurPyramidLevelView, BLUR_LEVEL_COUNT> levels{};

        BlurPyramidLevelView& Half() { return levels[0]; }
        BlurPyramidLevelView& Quarter() { return levels[1]; }
        BlurPyramidLevelView& Eighth() { return levels[2]; }
        BlurPyramidLevelView& Sixteenth() { return levels[3]; }
        BlurPyramidLevelView& ThirtySecond() { return levels[4]; }

        const BlurPyramidLevelView& Half() const { return levels[0]; }
        const BlurPyramidLevelView& Quarter() const { return levels[1]; }
        const BlurPyramidLevelView& Eighth() const { return levels[2]; }
        const BlurPyramidLevelView& Sixteenth() const { return levels[3]; }
        const BlurPyramidLevelView& ThirtySecond() const { return levels[4]; }

        void Reset()
        {
            for (auto& level : levels)
            {
                level = {};
            }
        }
    };

    struct TemporalHistoryNodePairView
    {
        RendererInterface::RenderGraphNodeHandle history_ab{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle history_ba{NULL_HANDLE};

        bool HasInit() const
        {
            return history_ab != NULL_HANDLE && history_ba != NULL_HANDLE;
        }

        RendererInterface::RenderGraphNodeHandle Select(bool history_read_is_a) const
        {
            return history_read_is_a ? history_ab : history_ba;
        }
    };

    struct CompositePassBundleView
    {
        RendererInterface::RenderGraphNodeHandle back{NULL_HANDLE};
        TemporalHistoryNodePairView front_history{};
        TemporalHistoryNodePairView single_history{};

        bool HasInit() const
        {
            return back != NULL_HANDLE &&
                   front_history.HasInit() &&
                   single_history.HasInit();
        }

        RendererInterface::RenderGraphNodeHandle SelectFront(bool history_read_is_a) const
        {
            return front_history.Select(history_read_is_a);
        }

        RendererInterface::RenderGraphNodeHandle SelectSingle(bool history_read_is_a) const
        {
            return single_history.Select(history_read_is_a);
        }
    };

    struct PanelPayloadTargets
    {
        RendererInterface::RenderTargetHandle mask_parameter{NULL_HANDLE};
        RendererInterface::RenderTargetHandle panel_optics{NULL_HANDLE};
        RendererInterface::RenderTargetHandle panel_profile{NULL_HANDLE};
        RendererInterface::RenderTargetHandle payload_depth{NULL_HANDLE};

        bool HasInit() const
        {
            return mask_parameter != NULL_HANDLE &&
                   panel_optics != NULL_HANDLE &&
                   panel_profile != NULL_HANDLE &&
                   payload_depth != NULL_HANDLE;
        }
    };

    struct PanelPayloadTargetBundle
    {
        PanelPayloadTargets primary{};
        PanelPayloadTargets secondary{};

        bool HasInit() const
        {
            return primary.HasInit() && secondary.HasInit();
        }

        void Reset()
        {
            primary = {};
            secondary = {};
        }
    };

    struct CompositeOutputTargets
    {
        RendererInterface::RenderTargetHandle final_output{NULL_HANDLE};
        RendererInterface::RenderTargetHandle back_composite{NULL_HANDLE};

        bool HasInit() const
        {
            return final_output != NULL_HANDLE && back_composite != NULL_HANDLE;
        }
    };

    struct PanelPayloadPassBundle
    {
        RendererInterface::RenderGraphNodeHandle compute{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle raster_front{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle raster_back{NULL_HANDLE};

        bool HasComputePath() const
        {
            return compute != NULL_HANDLE;
        }

        bool HasRasterPath() const
        {
            return raster_front != NULL_HANDLE && raster_back != NULL_HANDLE;
        }
    };

    struct InitDimensions
    {
        unsigned width{0};
        unsigned height{0};
        unsigned half_width{0};
        unsigned half_height{0};
        unsigned quarter_width{0};
        unsigned quarter_height{0};
        unsigned eighth_width{0};
        unsigned eighth_height{0};
        unsigned sixteenth_width{0};
        unsigned sixteenth_height{0};
        unsigned thirtysecond_width{0};
        unsigned thirtysecond_height{0};
        RendererInterface::ResourceUsage postfx_usage{};
    };

    struct InitBindings
    {
        RendererInterface::BufferBindingDesc panel_data{};
        RendererInterface::BufferBindingDesc global_params{};
    };

    struct TickRuntimePaths
    {
        BlurPyramidBundleView primary_blur{};
        BlurPyramidBundleView multilayer_blur{};
        CompositePassBundleView active_composite{};
        bool use_shared_mip_path{true};
        bool use_raster_panel_payload{false};
        bool use_strict_multilayer_path{false};
        RendererInterface::RenderGraphNodeHandle active_single_composite_pass{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle active_front_composite_pass{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle active_back_composite_pass{NULL_HANDLE};
    };

    struct TickExecutionPlan
    {
        unsigned render_width{0};
        unsigned render_height{0};
        TickRuntimePaths runtime_paths{};
        TickDispatchDimensions dispatch_dimensions{};
    };

    struct BlurRuntimeResolution
    {
        BlurSourceMode effective_mode{BlurSourceMode::SharedMip};
        bool use_shared_mip_path{true};
    };

    struct MultilayerRuntimeResolution
    {
        bool runtime_enabled{false};
        bool use_strict_multilayer_path{false};
    };

    struct TemporalHistoryState
    {
        RendererInterface::RenderTargetHandle history_a{NULL_HANDLE};
        RendererInterface::RenderTargetHandle history_b{NULL_HANDLE};
        bool read_is_a{true};
        bool force_reset{true};
        bool valid{false};

        bool HasTargets() const
        {
            return history_a != NULL_HANDLE && history_b != NULL_HANDLE;
        }

        RendererInterface::RenderTargetHandle ReadTarget() const
        {
            return read_is_a ? history_a : history_b;
        }

        RendererInterface::RenderTargetHandle WriteTarget() const
        {
            return read_is_a ? history_b : history_a;
        }

        void ResetRuntime()
        {
            read_is_a = true;
            force_reset = true;
            valid = false;
        }
    };

    struct DispatchCacheState
    {
        bool valid{false};
        unsigned render_width{0};
        unsigned render_height{0};
        bool path_shared_mip{true};
        bool path_raster_payload{false};
        bool path_strict_multilayer{false};

        bool Matches(
            unsigned width,
            unsigned height,
            bool shared_mip_path,
            bool raster_payload_path,
            bool strict_multilayer_path) const
        {
            return valid &&
                   render_width == width &&
                   render_height == height &&
                   path_shared_mip == shared_mip_path &&
                   path_raster_payload == raster_payload_path &&
                   path_strict_multilayer == strict_multilayer_path;
        }

        void Update(
            unsigned width,
            unsigned height,
            bool shared_mip_path,
            bool raster_payload_path,
            bool strict_multilayer_path)
        {
            valid = true;
            render_width = width;
            render_height = height;
            path_shared_mip = shared_mip_path;
            path_raster_payload = raster_payload_path;
            path_strict_multilayer = strict_multilayer_path;
        }

        void Reset()
        {
            valid = false;
        }
    };

    static unsigned ToInteractionStateIndex(PanelInteractionState state)
    {
        return static_cast<unsigned>(state);
    }

    void UploadPanelData(RendererInterface::ResourceOperator& resource_operator);
    void RefreshExternalPanelsFromProducers();
    static void EnsureDebugPanelOverrideCapacity(DebugPanelOverrideBucket& bucket, unsigned panel_count);
    static void ApplyDebugPanelOverrides(std::vector<FrostedGlassPanelDesc>& panel_descs, DebugPanelOverrideBucket& bucket);
    void SaveDebugPanelOverride(DebugPanelSource source, unsigned local_index, const FrostedGlassPanelDesc& panel_desc);
    void ApplyExternalPanelDebugOverrides();
    void ResetInitRuntimeState();
    InitDimensions BuildInitDimensions(RendererInterface::ResourceOperator& resource_operator) const;
    bool InitializeSharedPostFxResources(RendererInterface::ResourceOperator& resource_operator, const InitDimensions& dimensions);
    void CreateInitRenderTargets(RendererInterface::ResourceOperator& resource_operator, const InitDimensions& dimensions);
    void CreateInitBuffers(RendererInterface::ResourceOperator& resource_operator);
    InitBindings BuildInitBindings() const;
    bool CreatePrimaryBlurPyramidPasses(RendererInterface::ResourceOperator& resource_operator,
                                        RendererInterface::RenderGraph& graph,
                                        const InitDimensions& dimensions,
                                        const InitBindings& bindings,
                                        bool sync_existing = false);
    bool CreatePanelPayloadPasses(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph,
                                  const RendererSystemSceneRenderer::BasePassOutputs& scene_outputs,
                                  const InitDimensions& dimensions,
                                  const InitBindings& bindings,
                                  bool sync_existing = false);
    bool CreateCompositePasses(RendererInterface::ResourceOperator& resource_operator,
                               RendererInterface::RenderGraph& graph,
                               const RendererSystemSceneRenderer::BasePassOutputs& scene_outputs,
                               const InitDimensions& dimensions,
                               const InitBindings& bindings,
                               bool sync_existing = false);
    bool SyncStaticPassSetups(RendererInterface::ResourceOperator& resource_operator,
                              RendererInterface::RenderGraph& graph,
                              const RendererSystemSceneRenderer::BasePassOutputs& scene_outputs,
                              const InitDimensions& dimensions,
                              const InitBindings& bindings);
    void SyncTickTemporalHistoryFlags();
    BlurRuntimeResolution ResolveBlurRuntimeMode();
    MultilayerRuntimeResolution ResolveMultilayerRuntimeState(unsigned long long interval);
    void ResolveTickCompositePasses(TickRuntimePaths& runtime_paths) const;
    void ResolveTickPanelPayloadPath(TickRuntimePaths& runtime_paths);
    TickRuntimePaths BuildTickRuntimePaths(unsigned long long interval);
    TickExecutionPlan BuildTickExecutionPlan(unsigned long long interval, unsigned width, unsigned height);
    void UpdateTickRuntimeSummary(const TickRuntimePaths& runtime_paths);
    void RefreshTickPanelData(RendererInterface::ResourceOperator& resource_operator, float delta_seconds);
    void UpdateTickBlurDispatch(RendererInterface::RenderGraph& graph,
                                const BlurPyramidBundleView& blur_pyramid,
                                const TickDispatchDimensions& dispatch_dimensions,
                                bool use_shared_mip_path);
    void UpdateTickPanelPayloadDispatch(RendererInterface::RenderGraph& graph,
                                        const TickRuntimePaths& runtime_paths,
                                        const TickDispatchDimensions& dispatch_dimensions);
    void UpdateTickCompositeDispatch(RendererInterface::RenderGraph& graph,
                                     const TickRuntimePaths& runtime_paths,
                                     const TickDispatchDimensions& dispatch_dimensions);
    TickDispatchDimensions BuildTickDispatchDimensions(unsigned width, unsigned height) const;
    void UpdateTickDispatch(RendererInterface::RenderGraph& graph,
                            const TickExecutionPlan& execution_plan);
    bool RegisterTickBlurNodes(RendererInterface::RenderGraph& graph,
                               const BlurPyramidBundleView& blur_pyramid,
                               bool use_shared_mip_path) const;
    bool RegisterTickPanelPayloadNodes(RendererInterface::RenderGraph& graph,
                                       const TickRuntimePaths& runtime_paths) const;
    bool RegisterTickCompositeNodes(RendererInterface::RenderGraph& graph,
                                    const TickRuntimePaths& runtime_paths) const;
    bool RegisterTickRenderGraphNodes(RendererInterface::RenderGraph& graph,
                                      const TickExecutionPlan& execution_plan);
    void FinalizeTickHistoryState();
    void UpdateDirectionalHighlightParams();
    void UpdatePanelRuntimeStates(float delta_seconds);
    PanelStateCurve GetBlendedStateCurve(unsigned panel_index) const;
    FrostedGlassPanelGpuData ConvertPanelToGpuData(const FrostedGlassPanelDesc& panel_desc, const PanelStateCurve& blended_state_curve) const;
    BlurPyramidBundleView GetPrimaryBlurPyramidView() const;
    BlurPyramidBundleView GetMultilayerBlurPyramidView() const;
    CompositePassBundleView GetLegacyCompositePassBundleView() const;
    CompositePassBundleView GetSharedMipCompositePassBundleView() const;

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemLighting> m_lighting;
    BlurPyramidStorage m_primary_blur_pyramid{};
    PanelPayloadPassBundle m_panel_payload_passes{};
    CompositePassBundleView m_legacy_composite_passes{};
    BlurPyramidStorage m_multilayer_blur_pyramid{};
    CompositePassBundleView m_shared_mip_composite_passes{};
    CompositeOutputTargets m_composite_outputs{};
    PanelPayloadTargetBundle m_panel_payload_targets{};
    TemporalHistoryState m_temporal_history_state{};
    PostFxSharedResources m_postfx_shared_resources{};
    RendererInterface::BufferHandle m_frosted_panel_data_handle{NULL_HANDLE};
    RendererInterface::BufferHandle m_frosted_global_params_handle{NULL_HANDLE};

    FrostedGlassGlobalParams m_global_params{};
    std::vector<FrostedGlassPanelDesc> m_panel_descs;
    std::vector<ExternalPanelProducerEntry> m_external_panel_producers;
    std::vector<FrostedGlassPanelDesc> m_external_world_space_panel_descs;
    std::vector<FrostedGlassPanelDesc> m_external_overlay_panel_descs;
    std::vector<FrostedGlassPanelDesc> m_producer_world_space_panel_descs;
    std::vector<FrostedGlassPanelDesc> m_producer_overlay_panel_descs;
    std::vector<PanelRuntimeState> m_panel_runtime_states;
    unsigned m_next_external_panel_producer_id{1};
    unsigned m_last_upload_requested_panel_count{0};
    bool m_need_upload_panels{false};
    bool m_need_upload_global_params{true};
    BlurSourceMode m_blur_source_mode{BlurSourceMode::SharedMip};
    bool m_multilayer_runtime_enabled{true};
    unsigned m_multilayer_over_budget_streak{0};
    unsigned m_multilayer_cooldown_frames{0};
    PanelPayloadPath m_panel_payload_path{PanelPayloadPath::RasterPanelGBuffer};
    bool m_panel_payload_compute_fallback_active{false};
    unsigned m_last_expected_registered_pass_count{0};
    bool m_last_runtime_used_shared_mip_path{true};
    bool m_last_runtime_used_raster_payload{false};
    bool m_last_runtime_used_strict_multilayer{false};
    RenderFeature::FrameSizedSetupState m_static_pass_setup_state{};
    DispatchCacheState m_dispatch_cache_state{};
    unsigned m_debug_selected_panel_index{0};
    unsigned m_debug_selected_curve_state_index{0};
    DebugPanelOverrideBucket m_debug_override_producer_world{};
    DebugPanelOverrideBucket m_debug_override_producer_overlay{};
    DebugPanelOverrideBucket m_debug_override_manual_world{};
    DebugPanelOverrideBucket m_debug_override_manual_overlay{};
};
