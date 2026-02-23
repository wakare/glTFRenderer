#pragma once
#include <vector>
#include <string>

#include "RendererCommon.h"
#include "RendererInterface.h"

class RendererSystemBase;

class DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoBase)
    bool Init(const std::vector<std::string>& arguments);
    
    virtual void Run();
    
    bool InitRenderContext(const std::vector<std::string>& arguments);
    RendererInterface::ShaderHandle CreateShader(RendererInterface::ShaderType type, const std::string& source, const std::string& entry_function);
    RendererInterface::RenderTargetHandle CreateRenderTarget(
        const std::string& name,
        unsigned width,
        unsigned height,
        RendererInterface::PixelFormat format,
        RendererInterface::RenderTargetClearValue clear_value,
        RendererInterface::ResourceUsage usage);
    
protected:
    void TickFrame(unsigned long long time_interval);
    void DrawDebugUI();
    
    virtual bool InitInternal(const std::vector<std::string>& arguments) = 0;
    virtual void TickFrameInternal(unsigned long long time_interval);
    virtual void DrawDebugUIInternal() {}
    virtual const char* GetDemoPanelName() const { return "RendererDemo"; }
    
    unsigned m_width{1920};
    unsigned m_height{1080};
        
    std::shared_ptr<RendererInterface::RenderWindow> m_window;
    std::shared_ptr<RendererInterface::ResourceOperator> m_resource_manager;
    std::shared_ptr<RendererInterface::RenderGraph> m_render_graph;

    std::vector<std::shared_ptr<RendererInterface::RendererModuleBase>> m_modules;
    std::vector<std::shared_ptr<RendererSystemBase>> m_systems;
    std::map<RendererInterface::RenderTargetHandle, RendererInterface::RenderTargetDesc> m_render_target_desc_infos;
    unsigned m_last_render_width{0};
    unsigned m_last_render_height{0};
    RendererInterface::RenderDeviceType m_render_device_type{RendererInterface::DX12};
    RendererInterface::SwapchainResizePolicy m_swapchain_resize_policy_ui{};
    bool m_swapchain_resize_policy_ui_initialized{false};
};
