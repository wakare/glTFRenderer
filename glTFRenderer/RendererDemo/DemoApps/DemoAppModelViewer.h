#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererModule/RendererModuleSceneMesh.h"

class DemoAppModelViewer : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoAppModelViewer)
    
protected:
    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;
    
    std::shared_ptr<RendererModuleSceneMesh> m_scene_mesh_module;
    std::shared_ptr<RendererModuleCamera> m_camera_module;
    std::shared_ptr<RendererModuleLighting> m_lighting_module;
};
