#pragma once
#include <string>
#include <vector>

#include "IRHIShader.h"

class glTFShaderUtils
{
public:
    enum class ShaderFileType
    {
        SFT_HLSL,
        SFT_GLSL,
    };

    enum class ShaderType
    {
        ST_Vertex,
        ST_Fragment,
        ST_Compute,
        ST_Undefined,
    };
    
    struct ShaderCompileDesc
    {
        std::string file_path;
        std::string compile_target;
        std::string entry_function;
        RHIShaderPreDefineMacros shader_macros;
        bool spirv; // Vulkan binary format
        ShaderFileType file_type;
        ShaderType shader_type;
    };
    
    static bool IsShaderFileExist(const char* shaderFilePath);
    static bool LoadShaderFile(const char* shaderFilePath, std::string& outShaderFileContent);
    static std::string GetShaderCompileTarget(RHIShaderType type);
    static bool CompileShader(const ShaderCompileDesc& compile_desc, std::vector<unsigned char>& out_binaries);
};
