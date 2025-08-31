#pragma once
#include "Renderer.h"
#include "RendererInterface.h"

namespace RendererInterface
{
    class InternalResourceHandleTable
    {
    public:
        RenderWindowHandle RegisterWindow(const RenderWindow& window);
        const RenderWindow& GetRenderWindow(RenderWindowHandle handle) const;

        static InternalResourceHandleTable& Instance();
        
    protected:
        std::map<RenderWindowHandle, const RenderWindow&> m_windows;
        
    };
}
