#pragma once
#include <memory>

#include "glTFSceneGraph.h"
#include "glTFCamera/glTFCamera.h"

// Handle render scene graph with specific camera
class glTFSceneView
{
public:
    glTFSceneView(const glTFSceneGraph& graph);

    
    
private:
    const glTFSceneGraph& m_sceneGraph;
};
