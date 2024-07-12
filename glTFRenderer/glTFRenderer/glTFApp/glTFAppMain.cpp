#include "glTFAppMain.h"

#include <imgui.h>

#include "glTFGUIRenderer.h"
#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFPointLight.h"
#include "RenderWindow/glTFInputManager.h"
#include "RenderWindow/glTFWindow.h"

glTFTimer::glTFTimer()
    : m_delta_tick(0)
    , m_tick(GetTickCount64())
{
}

void glTFTimer::RecordFrameBegin()
{
    const size_t now = GetTickCount64();
    m_delta_tick = now - m_tick;
    m_tick = now;
}

size_t glTFTimer::GetDeltaFrameTimeMs() const
{
    return m_delta_tick;
}

glTFCmdArgumentProcessor::glTFCmdArgumentProcessor(int argc, char** argv)
    : raster_scene(true)
    , test_triangle_pass(false)
    , vulkan(false)
{
    for (int i = 0; i < argc; ++i)
    {
        std::string argument = argv[i];
        if (argument.find("raytracing") != std::string::npos)
        {
            raster_scene = false;
            continue;
        }

        if (argument.find("test_triangle_pass") != std::string::npos)
        {
            test_triangle_pass = true;
            continue;
        }

        if (argument.find("vulkan") != std::string::npos)
        {
            vulkan = true;
            continue;
        }
        
        const auto scene_name_index = argument.find("scene=");
        if (scene_name_index != std::string::npos)
        {
            scene_name = argument.substr(scene_name_index + 6);
            continue;
        }
    }
}

const std::string& glTFCmdArgumentProcessor::GetSceneName() const
{
    return scene_name;
}

glTFAppMain::glTFAppMain(int argc, char* argv[])
{
    // Parse command arguments
    const glTFCmdArgumentProcessor cmd_processor(argc, argv);
    m_app_config.use_rasterizer = cmd_processor.IsRasterScene();
    m_app_config.m_test_triangle_pass = cmd_processor.IsTestTrianglePass();
    m_app_config.m_vulkan = cmd_processor.IsVulkan();
    m_app_config.m_ReSTIR = true;
    m_app_config.m_recreate_renderer = true;
    m_app_config.m_tick_scene = true;
    
    // Init window
    auto& window = glTFWindow::Get();
    GLTF_CHECK(window.InitAndShowWindow());
    
    // Register window callback
    GLTF_CHECK(window.RegisterCallbackEventNative());
    
    m_scene_graph.reset(new glTFSceneGraph);
    InitSceneGraph(cmd_processor.GetSceneName());
    
    m_input_manager.reset(new glTFInputManager);
    window.SetInputManager(m_input_manager);
}

void glTFAppMain::Run()
{
    auto& window = glTFWindow::Get();
    {
        //Register window callback with App
        window.SetTickCallback([this](){
            InitRenderer();
        
            m_timer.RecordFrameBegin();
            const size_t time_delta_ms = m_timer.GetDeltaFrameTimeMs();
            
            if (m_app_config.m_tick_scene)
            {    
                m_scene_graph->Tick(time_delta_ms);    
            }

            m_renderer->TickRenderingBegin(time_delta_ms);
            m_renderer->TickSceneUpdating(*m_scene_graph, time_delta_ms);
            m_renderer->TickSceneRendering(*m_input_manager, time_delta_ms);
            m_renderer->TickGUIWidgetUpdate(time_delta_ms);
            m_renderer->TickRenderingEnd(time_delta_ms);
            m_input_manager->TickFrame(time_delta_ms);
        });

        window.SetExitCallback([this]()
        {
            // Clear all render resource
            m_renderer.reset(nullptr);
        });    
    }
    
    glTFWindow::Get().UpdateWindow();
}

bool glTFAppMain::InitSceneGraph(const std::string& scene_name)
{
    // Reset seed for random generator
    srand(1234);
    
    const bool loaded = LoadSceneGraphFromFile(scene_name.c_str());
    GLTF_CHECK(loaded);

    // Add camera
    std::unique_ptr<glTFSceneNode> camera_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(m_scene_graph->GetRootNode().m_finalTransform,
        90.0f, GetWidth(), GetHeight(), 0.001f, 1000.0f);
    
    camera_node->m_objects.push_back(std::move(camera));
    
    // Add light
    std::unique_ptr<glTFSceneNode> directional_light_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFDirectionalLight> directional_light = std::make_unique<glTFDirectionalLight>(directional_light_node->m_transform);
    directional_light->Rotate({glm::radians(45.0f), 0.0f, 0.0f});
    directional_light->SetIntensity(1.0f);
    directional_light->SetColor({1.0f, 1.0f,1.0f});
    directional_light->SetTickFunc([lightNode = directional_light.get()]()
    {
        lightNode->RotateOffset({0.0f, 0.001f, 0.0f});
    });
    directional_light_node->m_objects.push_back(std::move(directional_light));
    m_scene_graph->AddSceneNode(std::move(directional_light_node));

    static unsigned point_light_count = 0;
    glm::vec3 generator_min = {0.0f, 0.0f,0.0f};
    glm::vec3 generator_radius = {5.0f, 5.0f, 10.0f};
    std::unique_ptr<glTFSceneNode> point_light_node = std::make_unique<glTFSceneNode>();
    for (size_t i = 0; i < point_light_count; ++i)
    {
        std::unique_ptr<glTFPointLight> point_light = std::make_unique<glTFPointLight>(point_light_node->m_transform);
        glm::vec3 location = generator_min + glm::vec3{ generator_radius.x * Rand01(), generator_radius.y * Rand01(), generator_radius.z * Rand01()};
        point_light->SetColor({Rand01(), Rand01(), Rand01()});
        point_light->Translate(location);
        point_light->SetRadius(Rand01() * 100.0f);
        point_light->SetFalloff(Rand01());
        point_light->SetIntensity(Rand01() * 1.0f);
        /*
        point_light->SetTickFunc([light_node = point_light.get()]()
        {
            glm::vec3 position = glTF_Transform_WithTRS::GetTranslationFromMatrix(light_node->GetTransformMatrix());
            position += 0.01f * glm::vec3{2.0f * Rand01() - 1.0f, 2.0f * Rand01() - 1.0f, 2.0f * Rand01() - 1.0f};
            const glm::vec3 new_position = glm::clamp(position, {-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f});
            light_node->Translate(new_position);
        });
        */
        point_light_node->m_objects.push_back(std::move(point_light));
    }
    m_scene_graph->AddSceneNode(std::move(point_light_node));
    
    m_scene_graph->AddSceneNode(std::move(camera_node));

    return true;
}

bool glTFAppMain::LoadSceneGraphFromFile(const char* filePath) const
{
    glTFLoader loader;
    if (loader.LoadFile(filePath))
    {
        loader.Print();    
    }

    return m_scene_graph->Init(loader);
}

bool glTFAppMain::InitRenderer()
{
    if (!m_app_config.m_recreate_renderer)
    {
        return true;
    }
    
    m_app_config.m_recreate_renderer = false;
    if (m_renderer)
    {
        m_renderer->GetResourceManager().WaitAllFrameFinish();
    }
    m_renderer = nullptr; 

    glTFAppRendererConfig renderer_config;
    renderer_config.raster = m_app_config.use_rasterizer;
    renderer_config.ReSTIR = m_app_config.m_ReSTIR;
    renderer_config.test_triangle_pass = m_app_config.m_test_triangle_pass;
    renderer_config.vulkan = m_app_config.m_vulkan;
    
    m_renderer = std::make_unique<glTFAppRenderer>(renderer_config, glTFWindow::Get());
    m_renderer->InitScene(*m_scene_graph);
    m_renderer->GetGUIRenderer().AddWidgetSetupCallback([this](){UpdateGUIWidgets();});

    m_scene_graph->TraverseNodes([](const glTFSceneNode& node)
    {
        node.MarkDirty();
        return true;
    });
    
    return true;
}

unsigned glTFAppMain::GetWidth() const
{
    const auto& window = glTFWindow::Get();
    return window.GetWidth();
}

unsigned glTFAppMain::GetHeight() const
{
    const auto& window = glTFWindow::Get();
    return window.GetHeight();
}

bool glTFAppMain::UpdateGUIWidgets()
{
    constexpr ImVec4 subtitle_color = {1.0f, 0.0f, 1.0f, 1.0f};
    
    ImGui::Separator();
    ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "Config renderer settings");
    
    ImGui::Dummy({10.0f, 10.0f});
    ImGui::TextColored(subtitle_color, "Scene renderer type");
    if (ImGui::RadioButton("Rasterizer", m_app_config.use_rasterizer))
    {
        m_app_config.use_rasterizer = true;
    }

    if (ImGui::RadioButton("RayTracer", !m_app_config.use_rasterizer))
    {
        m_app_config.use_rasterizer = false;
    }
    
    if (!m_app_config.use_rasterizer)
    {
        ImGui::Dummy({10.0f, 10.0f});
        ImGui::TextColored(subtitle_color, "RayTracer lighting method");
        if (ImGui::RadioButton("ReSTIR_DI", m_app_config.m_ReSTIR))
        {
            m_app_config.m_ReSTIR = true;
        }

        if (ImGui::RadioButton("PathTracing", !m_app_config.m_ReSTIR))
        {
            m_app_config.m_ReSTIR = false;
        }
    }

    ImGui::Dummy({10.0f, 10.0f});
    ImGui::TextColored(subtitle_color, "Graphics API");
    if (ImGui::RadioButton("DX12", !m_app_config.m_vulkan))
    {
        m_app_config.m_vulkan = false;
    }
    
    if (ImGui::RadioButton("Vulkan", m_app_config.m_vulkan))
    {
        m_app_config.m_vulkan = true;
    }
    


    ImGui::Dummy({10.0f, 10.0f});
    ImGui::Checkbox("TickSceneUpdate", &m_app_config.m_tick_scene);

    ImGui::Dummy({10.0f, 10.0f});
    if (ImGui::Button("RecreateRenderer"))
    {
        m_app_config.m_recreate_renderer = true;
    }

    ImGui::Separator();
    
    return true;
}