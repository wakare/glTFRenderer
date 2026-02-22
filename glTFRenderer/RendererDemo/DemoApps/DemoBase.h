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
    struct RenderPassSetupInfo
    {
        struct ShaderSetupInfo
        {
            RendererInterface::ShaderType shader_type;
            std::string entry_function;
            std::string shader_file;
        };
        
        RendererInterface::RenderPassType render_pass_type;
        std::vector<ShaderSetupInfo> shader_setup_infos;
        std::map<RendererInterface::RenderTargetHandle, RendererInterface::RenderTargetBindingDesc> render_targets;
        std::map<RendererInterface::RenderTargetHandle, RendererInterface::RenderTargetTextureBindingDesc> sampled_render_targets;
        std::vector<std::shared_ptr<RendererInterface::RendererModuleBase>> modules;

        std::optional<RendererInterface::RenderExecuteCommand> execute_command;
        std::string debug_group;
        std::string debug_name;
    };
    
    void TickFrame(unsigned long long time_interval);
    RendererInterface::RenderGraphNodeHandle SetupPassNode(const RenderPassSetupInfo& setup_info);
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
};
