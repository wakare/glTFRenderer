#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemLighting.h"
#include "RendererSystemSceneRenderer.h"
#include "PostFxSharedResources.h"
#include <array>
#include <functional>
#include <glm/glm/glm.hpp>
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
        float depth_weight_scale{100.0f};
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
    unsigned GetEffectivePanelCount() const { return m_global_params.panel_count; }
    RendererInterface::RenderTargetHandle GetOutput() const { return m_frosted_pass_output; }
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
        unsigned multilayer_mode{1};
        float multilayer_overlap_threshold{0.08f};
        float multilayer_back_layer_weight{1.00f};
        float multilayer_front_transmittance{1.00f};
        unsigned multilayer_runtime_enabled{1};
        float multilayer_frame_budget_ms{22.0f};
        float blur_quarter_mix_boost{0.50f};
        float blur_response_scale{2.20f};
        float blur_sigma_normalization{5.5f};
        float depth_aware_min_strength{0.18f};
        float blur_veil_strength{1.35f};
        float blur_contrast_compression{0.90f};
        float blur_veil_tint_mix{0.55f};
        float blur_detail_preservation{0.04f};
        unsigned nan_debug_mode{0};
        float thickness_edge_power{2.20f};
        float thickness_highlight_boost_max{2.60f};
        float thickness_refraction_boost_max{1.90f};
        float thickness_edge_shadow_strength{0.20f};
        float thickness_range_min{0.004f};
        float thickness_range_max{0.060f};
        float edge_spec_intensity{0.95f};
        float edge_spec_sharpness{7.50f};
        float edge_spec_secondary_intensity{0.42f};
        float edge_spec_secondary_sharpness{22.0f};
        float edge_spec_luma_suppress{0.72f};
        float edge_spec_phase2_pad{0.0f};
        float edge_highlight_width{0.34f};
        float edge_highlight_white_mix{0.88f};
        float directional_highlight_min{0.10f};
        float directional_highlight_max{2.20f};
        float directional_highlight_curve{1.50f};
        glm::fvec4 highlight_light_dir_weight{0.0f, -1.0f, 0.0f, 0.0f}; // xyz: dominant directional light dir, w: valid flag
    };

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
    void UpdateDirectionalHighlightParams();
    void UpdatePanelRuntimeStates(float delta_seconds);
    PanelStateCurve GetBlendedStateCurve(unsigned panel_index) const;
    FrostedGlassPanelGpuData ConvertPanelToGpuData(const FrostedGlassPanelDesc& panel_desc, const PanelStateCurve& blended_state_curve) const;

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemLighting> m_lighting;

    RendererInterface::RenderGraphNodeHandle m_downsample_half_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_half_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_half_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_quarter_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_quarter_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_quarter_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_eighth_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_eighth_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_eighth_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_sixteenth_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_sixteenth_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_sixteenth_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_thirtysecond_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_thirtysecond_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_thirtysecond_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_mask_parameter_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_mask_parameter_raster_front_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_mask_parameter_raster_back_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_composite_back_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_half_multilayer_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_half_multilayer_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_half_multilayer_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_quarter_multilayer_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_quarter_multilayer_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_quarter_multilayer_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_eighth_multilayer_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_eighth_multilayer_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_eighth_multilayer_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_sixteenth_multilayer_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_sixteenth_multilayer_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_sixteenth_multilayer_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_downsample_thirtysecond_multilayer_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_thirtysecond_multilayer_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_thirtysecond_multilayer_vertical_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_composite_front_history_ab_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_composite_front_history_ba_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_composite_history_ab_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_composite_history_ba_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_pass_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_back_composite_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_mask_parameter_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_mask_parameter_secondary_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_panel_optics_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_panel_optics_secondary_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_panel_profile_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_panel_profile_secondary_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_panel_payload_depth{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_panel_payload_depth_secondary{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_half_multilayer_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_half_multilayer_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_quarter_multilayer_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_quarter_multilayer_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_eighth_blur_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_eighth_blur_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_sixteenth_blur_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_sixteenth_blur_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_thirtysecond_blur_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_thirtysecond_blur_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_eighth_multilayer_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_eighth_multilayer_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_sixteenth_multilayer_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_sixteenth_multilayer_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_thirtysecond_multilayer_ping{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_thirtysecond_multilayer_pong{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_half_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_quarter_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_eighth_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_sixteenth_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_thirtysecond_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_half_multilayer_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_quarter_multilayer_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_eighth_multilayer_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_sixteenth_multilayer_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_thirtysecond_multilayer_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_temporal_history_a{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_temporal_history_b{NULL_HANDLE};
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
    bool m_temporal_history_read_is_a{true};
    bool m_temporal_force_reset{true};
    bool m_temporal_history_valid{false};
    bool m_multilayer_runtime_enabled{true};
    unsigned m_multilayer_over_budget_streak{0};
    unsigned m_multilayer_cooldown_frames{0};
    PanelPayloadPath m_panel_payload_path{PanelPayloadPath::ComputeSDF};
    bool m_panel_payload_raster_ready{false};
    bool m_panel_payload_compute_fallback_active{false};
    unsigned m_debug_selected_panel_index{0};
    unsigned m_debug_selected_curve_state_index{0};
    DebugPanelOverrideBucket m_debug_override_producer_world{};
    DebugPanelOverrideBucket m_debug_override_producer_overlay{};
    DebugPanelOverrideBucket m_debug_override_manual_world{};
    DebugPanelOverrideBucket m_debug_override_manual_overlay{};
};
