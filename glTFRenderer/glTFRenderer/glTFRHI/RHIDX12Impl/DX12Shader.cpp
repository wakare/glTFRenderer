#include "DX12Shader.h"

#include <d3dcompiler.h>
#include <atlbase.h>

#include "glTFShaderUtils/glTFShaderUtils.h"
#include "glTFUtils/glTFUtils.h"

DX12Shader::DX12Shader()
= default;

DX12Shader::~DX12Shader()
= default;

const std::vector<unsigned char>& DX12Shader::GetShaderByteCode() const
{
    return m_shader_byte_code;
}

bool DX12Shader::InitShader(const std::string& shader_file_path, RHIShaderType type, const std::string& entry_function_name)
{
    assert(m_type == RHIShaderType::Unknown);
    m_type = type;
    m_shader_file_path = shader_file_path;
    
    m_shader_entry_function_name = entry_function_name;
    
    return true;
}

bool DX12Shader::CompileShader()
{
    glTFShaderUtils::ShaderCompileDesc compile_desc{};
    compile_desc.file_path = m_shader_file_path;
    compile_desc.entry_function = m_shader_entry_function_name;
    compile_desc.shader_macros = m_macros;
    compile_desc.compile_target = glTFShaderUtils::GetShaderCompileTarget(m_type);
    return glTFShaderUtils::CompileShader(compile_desc, m_shader_byte_code);
}