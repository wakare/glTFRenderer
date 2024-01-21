#include "glTFGUI.h"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIUtils.h"
#include "glTFWindow/glTFWindow.h"

bool glTFGUI::SetupGUIContext(const glTFWindow& window, glTFRenderResourceManager& resource_manager)
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

    m_descriptor_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    m_descriptor_heap->InitDescriptorHeap(resource_manager.GetDevice(),
        {
            1,
            RHIDescriptorHeapType::CBV_SRV_UAV,
            true
        }
    );
    
    RETURN_IF_FALSE(RHIUtils::Instance().InitGUIContext(resource_manager.GetDevice(), *m_descriptor_heap, resource_manager.GetBackBufferCount()))

    return true;
}

bool glTFGUI::RenderWidgets(glTFRenderResourceManager& resource_manager)
{
    // Rendering
    ImGui::Render();

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list,
        {&resource_manager.GetCurrentFrameSwapchainRT()}, nullptr))
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorHeapArray(command_list, m_descriptor_heap.get(), 1))
    RETURN_IF_FALSE(RHIUtils::Instance().RenderGUIFrame(command_list))

    resource_manager.CloseCommandListAndExecute(true);
    
    return true;
}

bool glTFGUI::SetupWidgetBegin()
{
    // Start the Dear ImGui frame
    RETURN_IF_FALSE(RHIUtils::Instance().NewGUIFrame())
    
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Setup constant widgets
    SetupWidgets();
    
    return true;
}

bool glTFGUI::SetupWidgetEnd()
{
    ImGui::End();
    
    return true;
}

bool glTFGUI::ExitAndClean()
{
    RETURN_IF_FALSE(RHIUtils::Instance().ExitGUI())
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    return true;
}

bool glTFGUI::HandleMouseEventThisFrame()
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool glTFGUI::HandleKeyBoardEventThisFrame()
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool glTFGUI::SetupWidgets()
{
    ImGui::Begin("glTFRenderer config");

    const auto& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    return true;
}
