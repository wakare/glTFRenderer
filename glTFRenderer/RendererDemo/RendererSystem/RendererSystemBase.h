#pragma once
#include "RendererInterface.h"

class RendererSystemBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RendererSystemBase)
    bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator);

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) = 0;
    virtual bool HasInit() const = 0;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator,RendererInterface::RenderGraph& graph, unsigned long long interval) = 0;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) {}
    virtual const char* GetSystemName() const { return "RendererSystem"; }
    virtual void DrawDebugUI() {}

protected:
    std::vector<std::shared_ptr<RendererInterface::RendererModuleBase>> m_modules;
};

template<typename type>
struct RendererSystemOutput
{
    RendererInterface::RenderTargetHandle GetRenderTargetHandle(const type& system, const std::string& name) {return NULL_HANDLE; }
};
