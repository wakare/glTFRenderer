#pragma once
#include <string>

struct MeshData
{
    
};

class MeshManager
{
public:
    bool LoadFile(const std::string& file_path);
    
protected:
    bool LoadFile_glTF(const std::string& file_path);    
};
