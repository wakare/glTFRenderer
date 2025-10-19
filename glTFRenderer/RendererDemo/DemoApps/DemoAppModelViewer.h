#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererModule/RendererModuleSceneMesh.h"

class DemoAppModelViewer : public DemoBase
{
public:
    virtual bool Init(const std::vector<std::string>& arguments) override;
    virtual void Run(const std::vector<std::string>& arguments) override;
    virtual void TickFrame(unsigned long long time_interval) override;
    
protected:
    std::shared_ptr<RendererModuleSceneMesh> m_scene_mesh_module;
    std::shared_ptr<RendererModuleCamera> m_camera_module;
    std::shared_ptr<RendererModuleLighting> m_lighting_module;
};
