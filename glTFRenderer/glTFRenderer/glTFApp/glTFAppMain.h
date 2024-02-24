#pragma once
#include "glTFAppRenderer.h"
#include "glTFVulkanTest.h"

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
    bool IsVulkanTest() const { return vulkan_test; }
    bool IsTestTrianglePass() const { return test_triangle_pass; }
    
private:
    bool raster_scene;
    bool vulkan_test;
    bool test_triangle_pass;
};

class glTFAppMain
{
public:
    glTFAppMain(int argc, char* argv[]);
    void Run();
    
protected:
    bool InitSceneGraph();
    bool LoadSceneGraphFromFile(const char* filePath) const;
    bool InitRenderer();

    unsigned GetWidth() const;
    unsigned GetHeight() const;

    bool UpdateGUIWidgets();

    // Only for test
    glTFVulkanTest vulkan_test;
    bool VulkanTestInit();
    bool VulkanTestUpdate();
    bool VulkanTestUnInit();

    std::shared_ptr<glTFInputManager> m_input_manager;

    glTFTimer m_timer;
    std::unique_ptr<glTFSceneGraph> m_scene_graph;
    std::unique_ptr<glTFAppRenderer> m_renderer;
    std::shared_ptr<glTFGUI> m_GUI;

    // GUI state
    bool m_raster_scene;
    bool m_ReSTIR;
    bool m_vulkan_test;
    bool m_test_triangle_pass;
    
    bool m_recreate_renderer;
};
