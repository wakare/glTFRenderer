#pragma once
#include "Renderer.h"
#include "RendererInterface.h"

class IRHIShader;

namespace RendererInterface
{
    class InternalResourceHandleTable
    {
    public:
        RenderWindowHandle RegisterWindow(const RenderWindow& window);
        const RenderWindow& GetRenderWindow(RenderWindowHandle handle) const;

        ShaderHandle RegisterShader(std::shared_ptr<IRHIShader> shader);
        std::shared_ptr<IRHIShader> GetShader(ShaderHandle handle) const;
        
        static InternalResourceHandleTable& Instance();
        
    protected:
        std::map<RenderWindowHandle, const RenderWindow&> m_windows;
        std::map<ShaderHandle, std::shared_ptr<IRHIShader>> m_shaders;
    };
}
