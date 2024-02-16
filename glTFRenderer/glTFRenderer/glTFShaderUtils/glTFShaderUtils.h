#pragma once
#include <string>

#include "glTFRHI/RHIInterface/IRHIShader.h"

class glTFShaderUtils
{
public:
    struct ShaderCompileDesc
    {
        std::string file_path;
        std::string compile_target;
        std::string entry_function;
        RHIShaderPreDefineMacros shader_macros;
        bool spirv; // Vulkan binary format
    };
    
    static bool IsShaderFileExist(const char* shaderFilePath);
    static bool LoadShaderFile(const char* shaderFilePath, std::string& outShaderFileContent);
    static std::string GetShaderCompileTarget(RHIShaderType type);
    static bool CompileShader(const ShaderCompileDesc& compile_desc, std::vector<unsigned char>& out_binaries);
};
