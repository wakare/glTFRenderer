#include "InternalResourceHandleTable.h"

static unsigned _internal_handle = 0;

RendererInterface::RenderWindowHandle RendererInterface::InternalResourceHandleTable::RegisterWindow(
    const RenderWindow& window)
{
    RenderWindowHandle result = _internal_handle++;
    m_windows.emplace(result, window);
    return result;
}

const RendererInterface::RenderWindow& RendererInterface::InternalResourceHandleTable::GetRenderWindow(
    RenderWindowHandle handle) const
{
    return m_windows.at(handle);
}
