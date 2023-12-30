#include "glTFAppMain.h"

#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFPointLight.h"
#include "glTFWindow/glTFInputManager.h"
#include "glTFWindow/glTFWindow.h"

float Rand01()
{
    return static_cast<float>(rand()) / RAND_MAX;
}

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
{
    for (int i = 0; i < argc; ++i)
    {
        const char* argument = argv[i];
        if (strcmp(argument, "raytracing") == 0)
        {
            raster_scene = false;
        }
    }
}

glTFAppMain::glTFAppMain(int argc, char* argv[])
{
    // Parse command arguments
    const glTFCmdArgumentProcessor cmd_processor(argc, argv);
    
    m_scene_graph.reset(new glTFSceneGraph);
    m_scene_renderer.reset(new glTFAppSceneRenderer(cmd_processor.IsRasterScene()));
    m_input_manager.reset(new glTFInputManager);
    
    // Init window
    auto& window = glTFWindow::Get();
    const unsigned width = window.GetWidth();
    const unsigned height = window.GetHeight();
    GLTF_CHECK(window.InitAndShowWindow());
    
    char file_path [MAX_PATH] = {'\0'};
    const std::string file_name = "Sponza";
    snprintf(file_path, sizeof(file_path), "glTFResources\\Models\\%s\\glTF\\%s.gltf", file_name.c_str(), file_name.c_str());
    const bool loaded = LoadSceneGraphFromFile(file_path);
    GLTF_CHECK(loaded);

    // Add camera
    std::unique_ptr<glTFSceneNode> camera_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(m_scene_graph->GetRootNode().m_finalTransform,
        90.0f, width, height, 0.001f, 1000.0f);
    
    camera_node->m_objects.push_back(std::move(camera));
    
    // Add light
    std::unique_ptr<glTFSceneNode> directional_light_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFDirectionalLight> directional_light = std::make_unique<glTFDirectionalLight>(directional_light_node->m_transform);
    directional_light->Rotate({glm::radians(45.0f), 0.0f, 0.0f});
    directional_light->SetIntensity(10.0f);
    directional_light->SetTickFunc([lightNode = directional_light.get()]()
    {
        lightNode->RotateOffset({0.0f, 0.001f, 0.0f});
    });
    directional_light_node->m_objects.push_back(std::move(directional_light));
    m_scene_graph->AddSceneNode(std::move(directional_light_node));

    std::unique_ptr<glTFSceneNode> point_light_node = std::make_unique<glTFSceneNode>();
    for (size_t i = 0; i < 5; ++i)
    {
        std::unique_ptr<glTFPointLight> point_light = std::make_unique<glTFPointLight>(point_light_node->m_transform);
        glm::fvec3 location = { 10.0f * Rand01() - 5.0f, 10.0f * Rand01() - 5.0f, 10.0f * Rand01() - 5.0f};
        point_light->Translate(location);
        point_light->SetRadius(Rand01() * 10.0f);
        point_light->SetFalloff(Rand01());
        point_light->SetIntensity(Rand01() * 10.0f);
        /*
        point_light->SetTickFunc([light_node = point_light.get()]()
        {
            glm::vec3 position = glTF_Transform_WithTRS::GetTranslationFromMatrix(light_node->GetTransformMatrix());
            position += 0.1f * glm::vec3{2.0f * Rand01() - 1.0f, 2.0f * Rand01() - 1.0f, 2.0f * Rand01() - 1.0f};
            const glm::vec3 new_position = glm::clamp(position, {-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f});
            light_node->Translate(new_position);
        });
        */
        point_light_node->m_objects.push_back(std::move(point_light));
    }
    m_scene_graph->AddSceneNode(std::move(point_light_node));
    
    m_scene_graph->AddSceneNode(std::move(camera_node));
}

void glTFAppMain::Run()
{
    auto& window = glTFWindow::Get();
    
    //Register window callback with App
    window.SetTickCallback([this](){
        m_timer.RecordFrameBegin();
        const size_t time_delta_ms = m_timer.GetDeltaFrameTimeMs();
        m_scene_graph->Tick(time_delta_ms);
        m_scene_renderer->TickFrame(*m_scene_graph, *m_input_manager, time_delta_ms);
        m_input_manager->TickFrame(time_delta_ms);
    });

    window.SetInputManager(m_input_manager);
    
    glTFWindow::Get().UpdateWindow();
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
