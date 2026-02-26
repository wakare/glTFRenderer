#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemFrostedGlass.h"

class RendererSystemFrostedPanelProducer : public RendererSystemBase
{
public:
    RendererSystemFrostedPanelProducer(std::shared_ptr<RendererSystemFrostedGlass> frosted);
    virtual ~RendererSystemFrostedPanelProducer() override;

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual const char* GetSystemName() const override { return "Frosted Panel Producer"; }
    virtual void DrawDebugUI() override;

    void SetWorldPanelDesc(const RendererSystemFrostedGlass::FrostedGlassPanelDesc& panel_desc);
    void SetOverlayPanelDesc(const RendererSystemFrostedGlass::FrostedGlassPanelDesc& panel_desc);
    void SetEnableWorldPanel(bool enabled) { m_enable_world_panel = enabled; }
    void SetEnableOverlayPanel(bool enabled) { m_enable_overlay_panel = enabled; }

private:
    bool RegisterProducer();
    void UnregisterProducer();

    std::shared_ptr<RendererSystemFrostedGlass> m_frosted;
    bool m_has_init{false};
    unsigned m_external_panel_producer_handle{0};
    bool m_enable_world_panel{true};
    bool m_enable_overlay_panel{true};
    RendererSystemFrostedGlass::FrostedGlassPanelDesc m_world_panel_desc{};
    RendererSystemFrostedGlass::FrostedGlassPanelDesc m_overlay_panel_desc{};
};
