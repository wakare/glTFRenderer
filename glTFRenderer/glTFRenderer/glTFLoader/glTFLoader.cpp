#include "glTFLoader.h"

#include <fstream>
#include "nlohmann_json/single_include/nlohmann/json.hpp"

bool glTFLoader::LoadFile(const char* file_path)
{
    std::ifstream glTF_file(file_path);
    if (glTF_file.bad())
    {
        return false;
    }
    
    nlohmann::json data = nlohmann::json::parse(glTF_file);
}
