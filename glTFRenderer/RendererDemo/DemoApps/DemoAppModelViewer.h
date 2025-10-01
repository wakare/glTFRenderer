#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"

class DemoAppModelViewer : public DemoBase
{
public:
    virtual void Run(const std::vector<std::string>& arguments) override;

protected:
    void TickFrame(unsigned long long time_interval);
    
    std::unique_ptr<RendererCamera> m_camera;
    RendererInterface::BufferHandle m_camera_buffer_handle{NULL_HANDLE};
};
