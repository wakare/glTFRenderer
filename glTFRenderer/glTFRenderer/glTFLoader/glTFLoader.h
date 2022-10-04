#pragma once
#include <memory>
#include <vector>

#include "glTFElementCommon.h"

class glTFLoader
{
public:
    bool LoadFile(const char* file_path);

private:
    
    std::vector<std::unique_ptr<glTF_Element_Base>> m_glTF_elements;
};
