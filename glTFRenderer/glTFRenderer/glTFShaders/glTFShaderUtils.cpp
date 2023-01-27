#include "glTFShaderUtils.h"
#include <fstream>

bool glTFShaderUtils::IsShaderFileExist(const char* shaderFilePath)
{
    const std::fstream shaderFileStream(shaderFilePath, std::ios::in);
    return shaderFileStream.good();
}

bool glTFShaderUtils::LoadShaderFile(const char* shaderFilePath, std::wstring& outShaderFileContent)
{
    std::wifstream shaderFileStream(shaderFilePath, std::ios::in);
    if (shaderFileStream.bad())
    {
        return false;
    }

    shaderFileStream >> outShaderFileContent;
    return true;
}
