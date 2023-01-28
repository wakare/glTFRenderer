#include "glTFShaderUtils.h"
#include <fstream>
#include <sstream>

bool glTFShaderUtils::IsShaderFileExist(const char* shaderFilePath)
{
    const std::fstream shaderFileStream(shaderFilePath, std::ios::in);
    return shaderFileStream.good();
}

bool glTFShaderUtils::LoadShaderFile(const char* shaderFilePath, std::string& outShaderFileContent)
{
    std::ifstream shaderFileStream(shaderFilePath, std::ios::in);
    if (shaderFileStream.bad())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << shaderFileStream.rdbuf();

    outShaderFileContent = buffer.str();
    
    return true;
}
