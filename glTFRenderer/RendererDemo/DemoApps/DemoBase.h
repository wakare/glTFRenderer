#pragma once
#include <vector>
#include <string>

#include "RendererInterface.h"

class DemoBase
{
public:
    virtual void Run(const std::vector<std::string>& arguments) = 0;
    
    void InitRenderContext(unsigned width, unsigned height, bool use_dx);
    RendererInterface::ShaderHandle CreateShader(RendererInterface::ShaderType type, const std::string& source, const std::string& entry_function);
    RendererInterface::RenderTargetHandle CreateRenderTarget(const std::string& name, unsigned width, unsigned height, RendererInterface::PixelFormat format, RendererInterface::RenderTargetClearValue clear_value, RendererInterface::ResourceUsage usage);
    
protected:
    std::shared_ptr<RendererInterface::RenderWindow> m_window;
    std::shared_ptr<RendererInterface::ResourceOperator> m_resource_manager;
    std::shared_ptr<RendererInterface::RenderGraph> m_render_graph;
};
