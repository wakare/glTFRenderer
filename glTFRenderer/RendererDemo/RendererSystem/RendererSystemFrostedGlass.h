#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemLighting.h"
#include "RendererSystemSceneRenderer.h"
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
        float custom_shape_index{0.0f};
        float pad0{0.0f};
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
    };

    struct FrostedGlassGlobalParams
    {
        unsigned panel_count{0};
        unsigned blur_radius{5};
        float scene_edge_scale{40.0f};
        float pad{0.0f};
    };

    void UploadPanelData(RendererInterface::ResourceOperator& resource_operator);
    FrostedGlassPanelGpuData ConvertPanelToGpuData(const FrostedGlassPanelDesc& panel_desc) const;

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemLighting> m_lighting;

    RendererInterface::RenderGraphNodeHandle m_frosted_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_frosted_pass_output{NULL_HANDLE};
    RendererInterface::BufferHandle m_frosted_panel_data_handle{NULL_HANDLE};
    RendererInterface::BufferHandle m_frosted_global_params_handle{NULL_HANDLE};

    FrostedGlassGlobalParams m_global_params{};
    std::vector<FrostedGlassPanelDesc> m_panel_descs;
    bool m_need_upload_panels{false};
    unsigned m_debug_selected_panel_index{0};
};
