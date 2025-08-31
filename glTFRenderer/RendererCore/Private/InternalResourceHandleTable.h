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
        
    protected:
        std::map<RenderWindowHandle, const RenderWindow&> m_windows;
    };

    static InternalResourceHandleTable s_internal_resource_handle_table;
}
