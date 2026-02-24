#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemLighting.h"
#include "RendererSystemSceneRenderer.h"
#include "PostFxSharedResources.h"
#include <array>
#include <glm/glm/glm.hpp>

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
        float corner_radius{0.03f};
        float blur_sigma{3.0f};
        float blur_strength{0.72f};
        float rim_intensity{0.10f};
        glm::fvec3 tint_color{0.92f, 0.96f, 1.0f};
        float depth_weight_scale{100.0f};
        PanelShapeType shape_type{PanelShapeType::RoundedRect};
        float edge_softness{1.0f};
        float thickness{0.02f};
        float refraction_strength{1.2f};
        float fresnel_intensity{0.10f};
        float fresnel_power{5.0f};
        float custom_shape_index{0.0f};
        float panel_alpha{1.0f};
        PanelInteractionState interaction_state{PanelInteractionState::Idle};
        float interaction_transition_speed{10.0f};
        float interaction_pad0{0.0f};
        float interaction_pad1{0.0f};
        std::array<PanelStateCurve, PANEL_INTERACTION_STATE_COUNT> state_curves{{
            {1.00f, 1.00f, 1.00f, 1.00f, 1.00f}, // Idle
            {1.20f, 1.08f, 1.20f, 1.15f, 1.00f}, // Hover
            {0.95f, 0.90f, 1.45f, 1.35f, 0.95f}, // Grab
            {1.35f, 1.20f, 1.10f, 1.05f, 0.90f}, // Move
            {1.45f, 1.25f, 1.25f, 1.20f, 0.90f}  // Scale
        }};
    };

    enum
    {
        MAX_PANEL_COUNT = 16
    };

    RendererSystemFrostedGlass(std::shared_ptr<RendererSystemSceneRenderer> scene,
                               std::shared_ptr<RendererSystemLighting> lighting);

    unsigned AddPanel(const FrostedGlassPanelDesc& panel_desc);
    bool UpdatePanel(unsigned index, const FrostedGlassPanelDesc& panel_desc);
    bool ContainsPanel(unsigned index) const;
    bool SetPanelInteractionState(unsigned index, PanelInteractionState interaction_state);
    PanelInteractionState GetPanelInteractionState(unsigned index) const;
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
        glm::fvec4 corner_blur_rim{};
        glm::fvec4 tint_depth_weight{};
        glm::fvec4 shape_info{};
        glm::fvec4 optical_info{};
    };

    struct FrostedGlassGlobalParams
    {
        unsigned panel_count{0};
        unsigned blur_radius{5};
        float scene_edge_scale{40.0f};
        float pad{0.0f};
    };

    struct PanelRuntimeState
    {
        PanelInteractionState target_state{PanelInteractionState::Idle};
        std::array<float, PANEL_INTERACTION_STATE_COUNT> state_weights{{1.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
    };

    static unsigned ToInteractionStateIndex(PanelInteractionState state)
    {
        return static_cast<unsigned>(state);
    }

    void UploadPanelData(RendererInterface::ResourceOperator& resource_operator);
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
    RendererInterface::RenderGraphNodeHandle m_frosted_mask_parameter_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_frosted_composite_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_pass_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_mask_parameter_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_panel_optics_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_half_blur_final_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_quarter_blur_final_output{NULL_HANDLE};
    PostFxSharedResources m_postfx_shared_resources{};
    RendererInterface::BufferHandle m_frosted_panel_data_handle{NULL_HANDLE};
    RendererInterface::BufferHandle m_frosted_global_params_handle{NULL_HANDLE};

    FrostedGlassGlobalParams m_global_params{};
    std::vector<FrostedGlassPanelDesc> m_panel_descs;
    std::vector<PanelRuntimeState> m_panel_runtime_states;
    bool m_need_upload_panels{false};
    unsigned m_debug_selected_panel_index{0};
    unsigned m_debug_selected_curve_state_index{0};
};
