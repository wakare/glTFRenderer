#pragma once
#include <memory>
#include <vector>

#include "glTFElementCommon.h"

class glTFLoader
{
public:
    glTFLoader();
    bool LoadFile(const char* file_path);

    void Print() const;
    
private:
    unsigned default_scene{};
    std::vector<std::unique_ptr<glTF_Element_Scene>> m_scenes;
    std::vector<std::unique_ptr<glTF_Element_Node>> m_nodes;
};
