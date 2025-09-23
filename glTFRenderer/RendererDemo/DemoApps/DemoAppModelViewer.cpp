#include "DemoAppModelViewer.h"

void DemoAppModelViewer::Run(const std::vector<std::string>& arguments)
{
    InitRenderContext(arguments);
    
    // Create shader resource
    auto vertex_shader_handle = CreateShader(RendererInterface::ShaderType::VERTEX_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainVS");
    auto fragment_shader_handle  = CreateShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainFS");
}
