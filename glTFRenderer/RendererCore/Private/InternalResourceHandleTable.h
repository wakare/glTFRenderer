#pragma once
#include "Renderer.h"
#include "RendererInterface.h"

class RendererSceneGraph;
class IRHIBufferAllocation;
class IRHITextureDescriptorAllocation;
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

        RenderTargetHandle RegisterRenderTarget(std::shared_ptr<IRHITextureDescriptorAllocation> render_target);
        std::shared_ptr<IRHITextureDescriptorAllocation> GetRenderTarget(RenderTargetHandle handle) const;

        RenderPassHandle RegisterRenderPass(std::shared_ptr<RenderPass> render_pass);
        std::shared_ptr<RenderPass> GetRenderPass(RenderPassHandle handle) const;

        BufferHandle RegisterBuffer(std::shared_ptr<IRHIBufferAllocation> buffer);
        std::shared_ptr<IRHIBufferAllocation> GetBuffer(BufferHandle handle) const;

        RenderSceneHandle RegisterRenderScene(std::shared_ptr<RendererSceneGraph> scene_graph);
        std::shared_ptr<RendererSceneGraph> GetRenderScene(RenderSceneHandle handle) const;
        
        static InternalResourceHandleTable& Instance();
        
    protected:
        std::map<RenderWindowHandle, const RenderWindow&> m_windows;
        std::map<ShaderHandle, std::shared_ptr<IRHIShader>> m_shaders;
        std::map<RenderTargetHandle, std::shared_ptr<IRHITextureDescriptorAllocation>> m_render_targets;
        std::map<RenderPassHandle, std::shared_ptr<RenderPass>> m_render_passes;
        std::map<BufferHandle, std::shared_ptr<IRHIBufferAllocation>> m_buffers;
        std::map<RenderSceneHandle, std::shared_ptr<RendererSceneGraph>> m_render_scene_graphs;
    };
}
