#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemFrostedGlass.h"

class RendererSystemFrostedPanelProducer : public RendererSystemBase
{
public:
    struct WorldPanelPrepassItem
    {
        glm::fvec3 world_center{0.0f, 1.20f, -0.90f};
        glm::fvec3 world_axis_u{0.56f, 0.0f, 0.0f};
        glm::fvec3 world_axis_v{0.0f, 0.34f, 0.0f};
        float layer_order{-0.5f};
        RendererSystemFrostedGlass::PanelDepthPolicy depth_policy{RendererSystemFrostedGlass::PanelDepthPolicy::SceneOcclusion};
        RendererSystemFrostedGlass::PanelShapeType shape_type{RendererSystemFrostedGlass::PanelShapeType::RoundedRect};
        float custom_shape_index{0.0f};
        float corner_radius{0.02f};
        float panel_alpha{1.0f};
    };

    struct OverlayPanelPrepassItem
    {
        glm::fvec2 center_uv{0.62f, 0.48f};
        glm::fvec2 half_size_uv{0.16f, 0.12f};
        float layer_order{1.0f};
        RendererSystemFrostedGlass::PanelShapeType shape_type{RendererSystemFrostedGlass::PanelShapeType::ShapeMask};
        float custom_shape_index{2.0f};
        float corner_radius{0.02f};
        float panel_alpha{1.0f};
    };

    RendererSystemFrostedPanelProducer(std::shared_ptr<RendererSystemFrostedGlass> frosted);
    virtual ~RendererSystemFrostedPanelProducer() override;

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual const char* GetSystemName() const override { return "Frosted Panel Producer"; }
    virtual void DrawDebugUI() override;

    void SetWorldPanelDesc(const RendererSystemFrostedGlass::FrostedGlassPanelDesc& panel_desc);
    void SetOverlayPanelDesc(const RendererSystemFrostedGlass::FrostedGlassPanelDesc& panel_desc);
    void SetWorldPanelPrepassItems(const std::vector<WorldPanelPrepassItem>& panel_items);
    void SetOverlayPanelPrepassItems(const std::vector<OverlayPanelPrepassItem>& panel_items);
    void ClearPrepassItems();
    void SetEnableWorldPanel(bool enabled) { m_enable_world_panel = enabled; }
    void SetEnableOverlayPanel(bool enabled) { m_enable_overlay_panel = enabled; }

private:
    RendererSystemFrostedGlass::FrostedGlassPanelDesc BuildWorldPanelDescFromPrepassItem(const WorldPanelPrepassItem& item) const;
    RendererSystemFrostedGlass::FrostedGlassPanelDesc BuildOverlayPanelDescFromPrepassItem(const OverlayPanelPrepassItem& item) const;
    bool RegisterProducer();
    void UnregisterProducer();

    std::shared_ptr<RendererSystemFrostedGlass> m_frosted;
    bool m_has_init{false};
    unsigned m_external_panel_producer_handle{0};
    bool m_enable_world_panel{true};
    bool m_enable_overlay_panel{true};
    RendererSystemFrostedGlass::FrostedGlassPanelDesc m_world_panel_desc{};
    RendererSystemFrostedGlass::FrostedGlassPanelDesc m_overlay_panel_desc{};
    std::vector<WorldPanelPrepassItem> m_world_panel_prepass_items{};
    std::vector<OverlayPanelPrepassItem> m_overlay_panel_prepass_items{};
};
