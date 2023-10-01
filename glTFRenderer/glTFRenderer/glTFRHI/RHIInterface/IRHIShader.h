#pragma once
#include <string>
#include <vector>

#include "IRHIResource.h"

enum class RHIShaderType
{
    Vertex,
    Pixel,
    Compute,
    RayTracing,
    Unknown,
};

struct RHIShaderPreDefineMacros
{
    void AddMacro(const std::string& key, const std::string& value)
    {
        macroKey.push_back(key);
        macroValue.push_back(value);
    }
    
    std::vector<std::string> macroKey;
    std::vector<std::string> macroValue;
};

struct RayTracingShaderEntryFunctionNames
{
    std::string raygen_shader_entry_name;
    std::string closest_hit_shader_entry_name;
    std::string miss_shader_entry_name;
    std::string any_hit_shader_entry_name;
};

class IRHIShader : public IRHIResource
{
public:
    IRHIShader(RHIShaderType type = RHIShaderType::Unknown);
    RHIShaderType GetType() const;
    const std::string& GetShaderContent() const;
    void SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros);
    
    virtual bool InitShader(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName, RayTracingShaderEntryFunctionNames raytracing_entry_name = RayTracingShaderEntryFunctionNames()) = 0;
    virtual bool CompileShader() = 0;
    
protected:
    bool LoadShader(const std::string& shaderFilePath);
    
    RHIShaderType m_type;
    std::string m_shader_file_path;
    
    RayTracingShaderEntryFunctionNames m_raytracing_entry_names;
    std::string m_shader_entry_function_name;
    std::string m_shader_content;
    std::string m_shaderFilePath;

    RHIShaderPreDefineMacros m_macros;
};