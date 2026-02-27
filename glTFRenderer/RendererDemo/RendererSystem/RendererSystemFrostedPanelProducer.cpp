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
    ImGui::Text("Prepass Items: world=%u overlay=%u",
                static_cast<unsigned>(m_world_panel_prepass_items.size()),
                static_cast<unsigned>(m_overlay_panel_prepass_items.size()));
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

void RendererSystemFrostedPanelProducer::SetWorldPanelPrepassItems(const std::vector<WorldPanelPrepassItem>& panel_items)
{
    m_world_panel_prepass_items = panel_items;
}

void RendererSystemFrostedPanelProducer::SetOverlayPanelPrepassItems(const std::vector<OverlayPanelPrepassItem>& panel_items)
{
    m_overlay_panel_prepass_items = panel_items;
}

void RendererSystemFrostedPanelProducer::ClearPrepassItems()
{
    m_world_panel_prepass_items.clear();
    m_overlay_panel_prepass_items.clear();
}

RendererSystemFrostedGlass::FrostedGlassPanelDesc RendererSystemFrostedPanelProducer::BuildWorldPanelDescFromPrepassItem(const WorldPanelPrepassItem& item) const
{
    RendererSystemFrostedGlass::FrostedGlassPanelDesc panel_desc = m_world_panel_desc;
    panel_desc.world_space_mode = 1u;
    panel_desc.depth_policy = item.depth_policy;
    panel_desc.world_center = item.world_center;
    panel_desc.world_axis_u = item.world_axis_u;
    panel_desc.world_axis_v = item.world_axis_v;
    panel_desc.layer_order = item.layer_order;
    panel_desc.shape_type = item.shape_type;
    panel_desc.custom_shape_index = item.custom_shape_index;
    panel_desc.corner_radius = item.corner_radius;
    panel_desc.panel_alpha = item.panel_alpha;
    return panel_desc;
}

RendererSystemFrostedGlass::FrostedGlassPanelDesc RendererSystemFrostedPanelProducer::BuildOverlayPanelDescFromPrepassItem(const OverlayPanelPrepassItem& item) const
{
    RendererSystemFrostedGlass::FrostedGlassPanelDesc panel_desc = m_overlay_panel_desc;
    panel_desc.world_space_mode = 0u;
    panel_desc.depth_policy = RendererSystemFrostedGlass::PanelDepthPolicy::Overlay;
    panel_desc.center_uv = item.center_uv;
    panel_desc.half_size_uv = item.half_size_uv;
    panel_desc.layer_order = item.layer_order;
    panel_desc.shape_type = item.shape_type;
    panel_desc.custom_shape_index = item.custom_shape_index;
    panel_desc.corner_radius = item.corner_radius;
    panel_desc.panel_alpha = item.panel_alpha;
    return panel_desc;
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
                if (!m_world_panel_prepass_items.empty())
                {
                    world_space_panels.reserve(world_space_panels.size() + m_world_panel_prepass_items.size());
                    for (const auto& item : m_world_panel_prepass_items)
                    {
                        world_space_panels.push_back(BuildWorldPanelDescFromPrepassItem(item));
                    }
                }
                else
                {
                    world_space_panels.push_back(m_world_panel_desc);
                }
            }
            if (m_enable_overlay_panel)
            {
                if (!m_overlay_panel_prepass_items.empty())
                {
                    overlay_panels.reserve(overlay_panels.size() + m_overlay_panel_prepass_items.size());
                    for (const auto& item : m_overlay_panel_prepass_items)
                    {
                        overlay_panels.push_back(BuildOverlayPanelDescFromPrepassItem(item));
                    }
                }
                else
                {
                    overlay_panels.push_back(m_overlay_panel_desc);
                }
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
