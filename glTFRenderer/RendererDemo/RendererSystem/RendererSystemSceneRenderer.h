#pragma once
#include "RendererSystemBase.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleSceneMesh.h"
#include <optional>
#include <string>

class RendererSystemSceneRenderer : public RendererSystemBase
{
    friend class RendererSystemOutput<RendererSystemSceneRenderer>;
public:
    RendererSystemSceneRenderer(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc, const std::string& scene_file);
    void UpdateInputDeviceInfo(RendererInputDevice& input_device, unsigned long long interval);

    unsigned GetWidth() const;
    unsigned GetHeight() const;
    const RendererInterface::RenderStateDesc& GetBasePassRenderState() const;
    bool SetBasePassRenderState(const RendererInterface::RenderStateDesc& render_state);
    
    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator,RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Scene Renderer"; }

    std::shared_ptr<RendererModuleCamera> GetCameraModule() const;
    std::shared_ptr<RendererModuleSceneMesh> GetSceneMeshModule() const;

protected:
    struct BasePassRuntimeState
    {
        RendererInterface::RenderTargetHandle color{NULL_HANDLE};
        RendererInterface::RenderTargetHandle normal{NULL_HANDLE};
        RendererInterface::RenderTargetHandle velocity{NULL_HANDLE};
        RendererInterface::RenderTargetHandle depth{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle node{NULL_HANDLE};

        void Reset();
        bool HasInit() const;
    };

    static RendererInterface::RenderStateDesc CreateDefaultBasePassRenderState();
    void CreateBasePassRenderTargets(RendererInterface::ResourceOperator& resource_operator);
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildBasePassSetupInfo() const;
    bool QueuePendingBasePassRenderStateUpdate(RendererInterface::RenderGraph& graph);

    std::shared_ptr<RendererModuleSceneMesh> m_scene_mesh_module;
    std::shared_ptr<RendererModuleCamera> m_camera_module;
    RendererInterface::RenderStateDesc m_base_pass_render_state{};
    std::optional<RendererInterface::RenderStateDesc> m_pending_base_pass_render_state{};
    BasePassRuntimeState m_base_pass_state{};
    RendererCameraDesc m_camera_desc{};
    std::string m_scene_file{};
};

template<>
struct RendererSystemOutput<RendererSystemSceneRenderer>
{
    RendererInterface::RenderTargetHandle GetRenderTargetHandle(const RendererSystemSceneRenderer& system, const std::string& name)
    {
        if (name == "m_base_pass_color")
        {
            return system.m_base_pass_state.color;
        }
        if (name == "m_base_pass_normal")
        {
            return system.m_base_pass_state.normal;
        }
        if (name == "m_base_pass_velocity")
        {
            return system.m_base_pass_state.velocity;
        }
        if (name == "m_base_pass_depth")
        {
            return system.m_base_pass_state.depth;
        }
        return NULL_HANDLE;
    }
};
