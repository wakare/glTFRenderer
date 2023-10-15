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

    void AddCBVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(b%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    void AddSRVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(t%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    void AddUAVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(u%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }

    void AddSamplerRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(s%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    std::vector<std::string> macroKey;
    std::vector<std::string> macroValue;
};

class IRHIShader : public IRHIResource
{
public:
    IRHIShader(RHIShaderType type = RHIShaderType::Unknown);
    RHIShaderType GetType() const;
    const std::string& GetShaderContent() const;
    void SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros);
    
    virtual bool InitShader(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName) = 0;
    virtual bool CompileShader() = 0;
 
protected:
    bool LoadShader(const std::string& shaderFilePath);
    
    RHIShaderType m_type;
    std::string m_shader_file_path;
    
    std::string m_shader_entry_function_name;
    std::string m_shader_content;
    std::string m_shaderFilePath;

    RHIShaderPreDefineMacros m_macros;
};