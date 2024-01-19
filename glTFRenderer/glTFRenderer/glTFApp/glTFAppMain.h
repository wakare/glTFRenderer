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

    bool IsRasterScene() const {return raster_scene; }

private:
    bool raster_scene;
};

class glTFAppMain
{
public:
    glTFAppMain(int argc, char* argv[]);
    void Run();
    
protected:
    bool InitSceneGraph();
    bool LoadSceneGraphFromFile(const char* filePath) const;

    unsigned GetWidth() const;
    unsigned GetHeight() const;
    
    std::shared_ptr<glTFInputManager> m_input_manager;

    glTFTimer m_timer;
    std::unique_ptr<glTFSceneGraph> m_scene_graph;
    std::unique_ptr<glTFAppRenderer> m_renderer;
    std::shared_ptr<glTFGUI> m_GUI;
};
