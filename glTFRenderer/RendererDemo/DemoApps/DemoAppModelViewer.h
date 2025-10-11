#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "SceneRendererUtil/SceneRendererCameraOperator.h"
#include "SceneRendererUtil/SceneRendererMeshDrawDispatcher.h"

class DemoAppModelViewer : public DemoBase
{
public:
    virtual void Run(const std::vector<std::string>& arguments) override;

protected:
    void TickFrame(unsigned long long time_interval);
    
    std::unique_ptr<SceneRendererMeshDrawDispatcher> m_draw_dispatcher;
    std::unique_ptr<SceneRendererCameraOperator> m_camera_operator;
    std::unique_ptr<RendererModuleLighting> m_lighting_module;
};
