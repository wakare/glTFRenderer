#pragma once
#include <string>

class glTFShaderUtils
{
public:
    static bool IsShaderFileExist(const char* shaderFilePath);
    static bool LoadShaderFile(const char* shaderFilePath, std::string& outShaderFileContent);
};
