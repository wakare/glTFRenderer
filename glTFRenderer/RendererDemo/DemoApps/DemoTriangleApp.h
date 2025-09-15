#pragma once
#include "DemoBase.h"
#include "RendererCommon.h"

class DemoTriangleApp : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoTriangleApp)
    
    virtual void Run(const std::vector<std::string>& arguments) override;
};
