#pragma once
#include "glTFAppRenderer.h"

class glTFTimer
{
public:
    glTFTimer();
    void RecordFrameBegin();
    size_t GetDeltaFrameTimeMs() const;

private:
    size_t m_delta_tick;
    size_t m_tick;
};

class glTFCmdArgumentProcessor
{
public:
    glTFCmdArgumentProcessor(int argc, char** argv);

    bool IsRasterScene() const { return raster_scene; }
    bool IsVulkan() const {return vulkan; }
    bool IsTestTrianglePass() const { return test_triangle_pass; }
    
    const std::string& GetSceneName() const;
    
private:
    bool raster_scene;
    bool test_triangle_pass;
    bool vulkan;
    
    std::string scene_name;
};

struct glTFAppConfig
{
    bool m_use_rasterizer;
    bool m_ReSTIR;
    bool m_test_triangle_pass;
    bool m_vulkan;
    bool m_virtual_texture;
    bool m_shadow;
    bool m_recreate_renderer;
    bool m_tick_scene;
};

class glTFAppMain
{
public:
    glTFAppMain(int argc, char* argv[]);
    void Run();
    
protected:
    bool InitSceneGraph(const std::string& scene_name);
    bool LoadSceneGraphFromFile(const char* file_path) const;
    bool InitRenderer();

    unsigned GetWidth() const;
    unsigned GetHeight() const;

    bool UpdateGUIWidgets();
    std::shared_ptr<glTFInputManager> m_input_manager;

    glTFTimer m_timer;
    std::shared_ptr<glTFSceneGraph> m_scene_graph;
    std::unique_ptr<glTFAppRenderer> m_renderer;

    // GUI state
    glTFAppConfig m_app_config{};
};
