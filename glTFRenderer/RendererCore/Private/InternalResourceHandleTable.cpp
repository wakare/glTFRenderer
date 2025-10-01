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

RendererInterface::ShaderHandle RendererInterface::InternalResourceHandleTable::RegisterShader(
    std::shared_ptr<IRHIShader> shader)
{
    RenderWindowHandle result = _internal_handle++;
    m_shaders.emplace(result, shader);
    return result;
}

std::shared_ptr<IRHIShader> RendererInterface::InternalResourceHandleTable::GetShader(ShaderHandle handle) const
{
    return m_shaders.at(handle);
}

RendererInterface::RenderTargetHandle RendererInterface::InternalResourceHandleTable::RegisterRenderTarget(
    std::shared_ptr<IRHITextureDescriptorAllocation> render_target)
{
    RenderWindowHandle result = _internal_handle++;
    m_render_targets.emplace(result, render_target);
    return result;
}

std::shared_ptr<IRHITextureDescriptorAllocation> RendererInterface::InternalResourceHandleTable::GetRenderTarget(
    RenderTargetHandle handle) const
{
    return m_render_targets.at(handle);
}

RendererInterface::RenderPassHandle RendererInterface::InternalResourceHandleTable::RegisterRenderPass(
    std::shared_ptr<RenderPass> render_pass)
{
    RenderWindowHandle result = _internal_handle++;
    m_render_passes.emplace(result, render_pass);
    return result;
}

std::shared_ptr<RenderPass> RendererInterface::InternalResourceHandleTable::GetRenderPass(RenderPassHandle handle) const
{
    return m_render_passes.at(handle);
}

RendererInterface::BufferHandle RendererInterface::InternalResourceHandleTable::RegisterBuffer(
    std::shared_ptr<IRHIBufferAllocation> buffer)
{
    BufferHandle result = _internal_handle++;
    m_buffers.emplace(result, buffer);
    return result;
}

std::shared_ptr<IRHIBufferAllocation> RendererInterface::InternalResourceHandleTable::GetBuffer(
    BufferHandle handle) const
{
    return m_buffers.at(handle);
}

RendererInterface::RenderSceneHandle RendererInterface::InternalResourceHandleTable::RegisterRenderScene(
    std::shared_ptr<RendererSceneGraph> scene_graph)
{
    RenderSceneHandle result = _internal_handle++;
    m_render_scene_graphs.emplace(result, scene_graph);
    return result;
}

std::shared_ptr<RendererSceneGraph> RendererInterface::InternalResourceHandleTable::GetRenderScene(
    RenderSceneHandle handle) const
{
    return m_render_scene_graphs.at(handle);
}

RendererInterface::IndexedBufferHandle RendererInterface::InternalResourceHandleTable::RegisterIndexedBufferAndView(
    std::shared_ptr<IRHIIndexBufferView> buffer_view, std::shared_ptr<RHIIndexBuffer> buffer)
{
    RenderSceneHandle result = _internal_handle++;
    m_indexed_buffer_views.emplace(result, buffer_view);
    m_indexed_buffers.emplace(result, buffer);
    return result;
}

std::shared_ptr<RHIIndexBuffer> RendererInterface::InternalResourceHandleTable::GetIndexBuffer(
    IndexedBufferHandle handle) const
{
    return m_indexed_buffers.at(handle);
}

std::shared_ptr<IRHIIndexBufferView> RendererInterface::InternalResourceHandleTable::GetIndexBufferView(
    IndexedBufferHandle handle) const
{
    return m_indexed_buffer_views.at(handle);
}

RendererInterface::InternalResourceHandleTable& RendererInterface::InternalResourceHandleTable::Instance()
{
    static InternalResourceHandleTable s_internal_resource_handle_table;
    return s_internal_resource_handle_table;
}
