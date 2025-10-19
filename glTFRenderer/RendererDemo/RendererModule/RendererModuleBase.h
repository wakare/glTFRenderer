#pragma once
#include "RendererCommon.h"

namespace RendererInterface
{
    struct RenderPassDrawDesc;
    class ResourceOperator;
}

class RendererModuleBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RendererModuleBase)
    virtual bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator) = 0;
    virtual bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) = 0;
    virtual bool Tick(RendererInterface::ResourceOperator&, unsigned long long interval) {return true; };
};
