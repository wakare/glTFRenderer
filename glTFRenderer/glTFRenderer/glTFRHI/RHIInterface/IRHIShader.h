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

    void AddCBVRegisterDefine(const std::string& key, unsigned register_index)
    {
        char format_string[16] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(b%u)", register_index);
        AddMacro(key, format_string);
    }
    
    void AddSRVRegisterDefine(const std::string& key, unsigned register_index)
    {
        char format_string[16] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(t%u)", register_index);
        AddMacro(key, format_string);
    }
    
    void AddUAVRegisterDefine(const std::string& key, unsigned register_index)
    {
        char format_string[16] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(u%u)", register_index);
        AddMacro(key, format_string);
    }

    void AddSamplerRegisterDefine(const std::string& key, unsigned register_index)
    {
        char format_string[16] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(s%u)", register_index);
        AddMacro(key, format_string);
    }
    
    std::vector<std::string> macroKey;
    std::vector<std::string> macroValue;
};

struct RayTracingShaderEntryFunctionNames
{
    std::string raygen_shader_entry_name;
    std::string miss_shader_entry_name;
    std::string closest_hit_shader_entry_name;
    std::string any_hit_shader_entry_name;
    std::string intersection_shader_entry_name;

    std::vector<std::string> GetValidEntryFunctionNames() const
    {
        std::vector<std::string> result;
        
        if (!raygen_shader_entry_name.empty())
        {
            result.push_back(raygen_shader_entry_name);
        }

        if (!miss_shader_entry_name.empty())
        {
            result.push_back(miss_shader_entry_name);
        }

        if (!closest_hit_shader_entry_name.empty())
        {
            result.push_back(closest_hit_shader_entry_name);
        }

        if (!any_hit_shader_entry_name.empty())
        {
            result.push_back(any_hit_shader_entry_name);
        }
        
        return result;
    }
};

class IRHIShader : public IRHIResource
{
public:
    IRHIShader(RHIShaderType type = RHIShaderType::Unknown);
    RHIShaderType GetType() const;
    const std::string& GetShaderContent() const;
    void SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros);
    
    virtual bool InitShader(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName, const
                            RayTracingShaderEntryFunctionNames& raytracing_entry_name = RayTracingShaderEntryFunctionNames()) = 0;
    virtual bool CompileShader() = 0;
 
    const RayTracingShaderEntryFunctionNames& GetRayTracingEntryFunctionNames() const { return m_raytracing_entry_names; }
    
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