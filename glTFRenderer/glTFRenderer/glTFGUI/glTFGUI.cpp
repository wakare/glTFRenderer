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
    //ImGui::StyleColorsLight();

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

bool glTFGUI::RenderNewFrame(glTFRenderResourceManager& resource_manager)
{
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Start the Dear ImGui frame
    RETURN_IF_FALSE(RHIUtils::Instance().NewGUIFrame())
    
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        //ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        //ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

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
