#include "RendererSystemFrostedPanelProducer.h"
#include <imgui/imgui.h>
#include <utility>

RendererSystemFrostedPanelProducer::RendererSystemFrostedPanelProducer(std::shared_ptr<RendererSystemFrostedGlass> frosted)
    : m_frosted(std::move(frosted))
{
}

RendererSystemFrostedPanelProducer::~RendererSystemFrostedPanelProducer()
{
    UnregisterProducer();
}

bool RendererSystemFrostedPanelProducer::Init(RendererInterface::ResourceOperator& resource_operator,
                                              RendererInterface::RenderGraph& graph)
{
    (void)resource_operator;
    (void)graph;
    GLTF_CHECK(m_frosted);
    m_has_init = RegisterProducer();
    return m_has_init;
}

bool RendererSystemFrostedPanelProducer::HasInit() const
{
    return m_has_init && m_external_panel_producer_handle != 0;
}

bool RendererSystemFrostedPanelProducer::Tick(RendererInterface::ResourceOperator& resource_operator,
                                              RendererInterface::RenderGraph& graph,
                                              unsigned long long interval)
{
    (void)resource_operator;
    (void)graph;
    (void)interval;
    if (!m_has_init)
    {
        return false;
    }
    if (m_external_panel_producer_handle == 0)
    {
        m_has_init = RegisterProducer();
    }
    return m_external_panel_producer_handle != 0;
}

void RendererSystemFrostedPanelProducer::DrawDebugUI()
{
    ImGui::Checkbox("Producer World Panel", &m_enable_world_panel);
    ImGui::Checkbox("Producer Overlay Panel", &m_enable_overlay_panel);
    ImGui::Text("Producer Handle: %u", m_external_panel_producer_handle);
}

void RendererSystemFrostedPanelProducer::SetWorldPanelDesc(const RendererSystemFrostedGlass::FrostedGlassPanelDesc& panel_desc)
{
    m_world_panel_desc = panel_desc;
}

void RendererSystemFrostedPanelProducer::SetOverlayPanelDesc(const RendererSystemFrostedGlass::FrostedGlassPanelDesc& panel_desc)
{
    m_overlay_panel_desc = panel_desc;
}

bool RendererSystemFrostedPanelProducer::RegisterProducer()
{
    if (!m_frosted || m_external_panel_producer_handle != 0)
    {
        return m_external_panel_producer_handle != 0;
    }

    m_external_panel_producer_handle = m_frosted->RegisterExternalPanelProducer(
        [this](std::vector<RendererSystemFrostedGlass::FrostedGlassPanelDesc>& world_space_panels,
               std::vector<RendererSystemFrostedGlass::FrostedGlassPanelDesc>& overlay_panels)
        {
            if (m_enable_world_panel)
            {
                world_space_panels.push_back(m_world_panel_desc);
            }
            if (m_enable_overlay_panel)
            {
                overlay_panels.push_back(m_overlay_panel_desc);
            }
        });
    return m_external_panel_producer_handle != 0;
}

void RendererSystemFrostedPanelProducer::UnregisterProducer()
{
    if (!m_frosted || m_external_panel_producer_handle == 0)
    {
        return;
    }
    m_frosted->UnregisterExternalPanelProducer(m_external_panel_producer_handle);
    m_external_panel_producer_handle = 0;
}
