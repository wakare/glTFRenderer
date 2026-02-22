#pragma once
#include "RendererSystemBase.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleSceneMesh.h"

class RendererSystemSceneRenderer : public RendererSystemBase
{
    friend class RendererSystemOutput<RendererSystemSceneRenderer>;
public:
    RendererSystemSceneRenderer(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc, const std::string& scene_file);
    void UpdateInputDeviceInfo(RendererInputDevice& input_device, unsigned long long interval);

    unsigned GetWidth() const;
    unsigned GetHeight() const;
    
    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator,RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Scene Renderer"; }

    std::shared_ptr<RendererModuleCamera> GetCameraModule() const;
    std::shared_ptr<RendererModuleSceneMesh> GetSceneMeshModule() const;

protected:
    std::shared_ptr<RendererModuleSceneMesh> m_scene_mesh_module;
    std::shared_ptr<RendererModuleCamera> m_camera_module;

    RendererInterface::RenderTargetHandle m_base_pass_color{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_base_pass_normal{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_base_pass_depth{NULL_HANDLE};

    RendererInterface::RenderGraphNodeHandle m_base_pass_node{NULL_HANDLE};
};

template<>
struct RendererSystemOutput<RendererSystemSceneRenderer>
{
    RendererInterface::RenderTargetHandle GetRenderTargetHandle(const RendererSystemSceneRenderer& system, const std::string& name)
    {
        if (name == "m_base_pass_color")
        {
            return system.m_base_pass_color;
        }
        if (name == "m_base_pass_normal")
        {
            return system.m_base_pass_normal;
        }
        if (name == "m_base_pass_depth")
        {
            return system.m_base_pass_depth;
        }
        return NULL_HANDLE;
    }
};
