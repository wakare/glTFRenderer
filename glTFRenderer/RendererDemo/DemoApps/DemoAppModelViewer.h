#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererSystem/RendererSystemFrostedGlass.h"
#include "RendererSystem/RendererSystemFrostedPanelProducer.h"
#include "RendererSystem/RendererSystemLighting.h"
#include "RendererSystem/RendererSystemSceneRenderer.h"
#include "RendererSystem/RendererSystemToneMap.h"
#include <vector>

class DemoAppModelViewer : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoAppModelViewer)
    
protected:
    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;
    virtual void DrawDebugUIInternal() override;
    void UpdateFrostedPanelPrepassFeeds(float timeline_seconds);
    bool ExportB7AcceptanceSnapshot();

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemLighting> m_lighting;
    std::shared_ptr<RendererSystemFrostedPanelProducer> m_frosted_panel_producer;
    std::shared_ptr<RendererSystemFrostedGlass> m_frosted_glass;
    std::shared_ptr<RendererSystemToneMap> m_tone_map;

    unsigned m_directional_light_index{0};
    LightInfo m_directional_light_info{};
    float m_directional_light_elapsed_seconds{0.0f};
    float m_directional_light_angular_speed_radians{0.25f};
    bool m_enable_panel_input_state_machine{true};
    bool m_enable_frosted_prepass_feeds{true};
    bool m_auto_export_b7_snapshot{false};
    bool m_auto_export_b7_snapshot_done{false};
    int m_b7_snapshot_warmup_frames{180};
    std::string m_last_b7_snapshot_summary_path{};
    std::string m_last_b7_snapshot_csv_path{};
    std::vector<RendererSystemFrostedPanelProducer::WorldPanelPrepassItem> m_world_prepass_panels{};
    std::vector<RendererSystemFrostedPanelProducer::OverlayPanelPrepassItem> m_overlay_prepass_panels{};
};
