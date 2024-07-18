#include "glTFGUIRenderer.h"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIUtils.h"
#include "RenderWindow/glTFWindow.h"

glTFGUIRenderer::~glTFGUIRenderer()
{
    ExitAndClean();
}

bool glTFGUIRenderer::SetupGUIContext(const glTFWindow& window, glTFRenderResourceManager& resource_manager)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window.m_glfw_window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif

    RETURN_IF_FALSE(RHIUtils::Instance().InitGUIContext(resource_manager.GetDevice(), resource_manager.GetCommandQueue(), resource_manager.GetMemoryManager().GetDescriptorManager(), resource_manager.GetBackBufferCount()))

    glTFWindow::Get().SetInputHandleCallback([this](){return HandleMouseEventThisFrame();});
    m_inited = true;
    
    return true;
}

bool glTFGUIRenderer::RenderWidgets(glTFRenderResourceManager& resource_manager)
{
    // Rendering
    ImGui::Render();

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIBeginRenderingInfo begin_rendering_info{};
    begin_rendering_info.rendering_area_width = resource_manager.GetSwapChain().GetWidth();
    begin_rendering_info.rendering_area_height = resource_manager.GetSwapChain().GetHeight();
    begin_rendering_info.enable_depth_write = false;
    begin_rendering_info.m_render_targets = {&resource_manager.GetCurrentFrameSwapChainRTV()};
    
    RETURN_IF_FALSE(RHIUtils::Instance().BeginRendering(command_list, begin_rendering_info));
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().BindGUIDescriptorContext(command_list))
    RETURN_IF_FALSE(RHIUtils::Instance().RenderGUIFrame(command_list))
    RETURN_IF_FALSE(RHIUtils::Instance().EndRendering(command_list));
    
    resource_manager.CloseCommandListAndExecute({}, true);
    
    return true;
}

bool glTFGUIRenderer::UpdateWidgets()
{
    // Start the Dear ImGui frame
    RETURN_IF_FALSE(RHIUtils::Instance().NewGUIFrame())
    
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("glTFRenderer config");
    
    SetupWidgetsInternal();
    for (const auto& setup_callback : m_widget_setup_callbacks)
    {
        setup_callback();
    }
    
    ImGui::End();
    
    ImGui::EndFrame();
    
    return true;
}

bool glTFGUIRenderer::ExitAndClean()
{
    m_widget_setup_callbacks.clear();
    RETURN_IF_FALSE(RHIUtils::Instance().ExitGUI())
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    //Reset input handle callback
    glTFWindow::Get().SetInputHandleCallback([](){return false;});
    
    return true;
}

bool glTFGUIRenderer::HandleMouseEventThisFrame() const
{
    return m_inited ? ImGui::GetIO().WantCaptureMouse : false;
}

bool glTFGUIRenderer::HandleKeyBoardEventThisFrame() const
{
    return m_inited ? ImGui::GetIO().WantCaptureKeyboard : false;
}

bool glTFGUIRenderer::AddWidgetSetupCallback(GUIWidgetSetupCallback callback)
{
    m_widget_setup_callbacks.push_back(std::move(callback));
    return true;
}

bool glTFGUIRenderer::SetupWidgetsInternal()
{
    ImGui::Separator();
    ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "GUI state");
    
    const auto& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    ImGui::Separator();
    
    return true;
}
