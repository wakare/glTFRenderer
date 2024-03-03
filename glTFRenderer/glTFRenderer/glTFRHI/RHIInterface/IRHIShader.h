#pragma once
#include <string>
#include <vector>

#include "IRHIResource.h"

class IRHIShader : public IRHIResource
{
public:
    IRHIShader(RHIShaderType type = RHIShaderType::Unknown);
    RHIShaderType GetType() const;
    const std::string& GetShaderContent() const;
    const std::string& GetMainEntry() const;
    void SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros);
    
    bool InitShader(const std::string& shader_file_path, RHIShaderType type, const std::string& entry_function_name);
    bool CompileShader();
    
    const std::vector<unsigned char>& GetShaderByteCode() const;
    
protected:
    virtual bool CompileSpirV() = 0;
    bool LoadShader(const std::string& shaderFilePath);
    
    RHIShaderType m_type;
    std::string m_shader_file_path;
    
    std::string m_shader_entry_function_name;
    std::string m_shader_content;
    std::string m_shaderFilePath;

    RHIShaderPreDefineMacros m_macros;
    std::vector<unsigned char> m_shader_byte_code;
};