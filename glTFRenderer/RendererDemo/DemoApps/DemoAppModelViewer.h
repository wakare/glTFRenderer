#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererSystem/RendererSystemLighting.h"
#include "RendererSystem/RendererSystemSceneRenderer.h"

class DemoAppModelViewer : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoAppModelViewer)
    
protected:
    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemLighting> m_lighting;
};
