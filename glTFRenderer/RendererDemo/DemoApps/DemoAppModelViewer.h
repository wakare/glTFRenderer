#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererSystem/RendererSystemLighting.h"
#include "RendererSystem/RendererSystemSceneRenderer.h"
#include "RendererSystem/RendererSystemToneMap.h"
#include <memory>
#include <string>
#include <vector>

class RendererSystemFrostedGlass;

class DemoAppModelViewer : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoAppModelViewer)

protected:
    struct ModelViewerStateSnapshot : NonRenderStateSnapshot
    {
        bool has_camera_pose{false};
        glm::fvec3 camera_position{0.0f};
        glm::fvec3 camera_euler_angles{0.0f};
        unsigned camera_viewport_width{0};
        unsigned camera_viewport_height{0};

        unsigned directional_light_index{0};
        LightInfo directional_light_info{};
        float directional_light_elapsed_seconds{0.0f};
        float directional_light_speed_radians{0.25f};
    };

    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual bool RebuildRenderRuntimeObjects() override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;
    virtual void DrawDebugUIInternal() override;
    virtual std::shared_ptr<NonRenderStateSnapshot> CaptureNonRenderStateSnapshot() const override;
    virtual bool ApplyNonRenderStateSnapshot(const std::shared_ptr<NonRenderStateSnapshot>& snapshot) override;
    virtual bool SerializeNonRenderStateSnapshotToJson(
        const std::shared_ptr<NonRenderStateSnapshot>& snapshot,
        nlohmann::json& out_snapshot_json) const override;
    virtual std::shared_ptr<NonRenderStateSnapshot> DeserializeNonRenderStateSnapshotFromJson(
        const nlohmann::json& snapshot_json,
        std::string& out_error) const override;
    virtual const char* GetSnapshotTypeName() const override { return "DemoAppModelViewer"; }

    bool RebuildModelViewerBaseRuntimeObjects();
    bool FinalizeModelViewerRuntimeObjects(const std::shared_ptr<RendererSystemFrostedGlass>& tone_map_frosted_input);
    void UpdateModelViewerFrame(unsigned long long time_interval,
                                bool update_scene_input = true,
                                bool freeze_directional_light = false);
    void DrawModelViewerBaseDebugUI();
    std::shared_ptr<ModelViewerStateSnapshot> CaptureModelViewerStateSnapshot() const;
    bool ApplyModelViewerStateSnapshot(const ModelViewerStateSnapshot& snapshot);

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemLighting> m_lighting;
    std::shared_ptr<RendererSystemToneMap> m_tone_map;

    unsigned m_directional_light_index{0};
    LightInfo m_directional_light_info{};
    float m_directional_light_elapsed_seconds{0.0f};
    float m_directional_light_angular_speed_radians{0.25f};
};
