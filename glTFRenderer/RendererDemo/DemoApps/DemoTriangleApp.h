#pragma once
#include <glm/glm/gtx/compatibility.hpp>

#include "DemoBase.h"
#include "RendererCommon.h"

class DemoTriangleApp : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoTriangleApp)

protected:
    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;
    
    glm::float4 m_color;
    RendererInterface::BufferHandle m_color_buffer_handle;
};
