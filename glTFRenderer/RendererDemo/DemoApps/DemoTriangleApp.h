#pragma once
#include <glm/glm/gtx/compatibility.hpp>

#include "DemoBase.h"
#include "RendererCommon.h"

class DemoTriangleApp : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoTriangleApp)
    virtual bool Init(const std::vector<std::string>& arguments) override;
    virtual void Run(const std::vector<std::string>& arguments) override;
    virtual void TickFrame(unsigned long long time_interval) override;

protected:
    glm::float4 m_color;
    RendererInterface::BufferHandle m_color_buffer_handle;
};
