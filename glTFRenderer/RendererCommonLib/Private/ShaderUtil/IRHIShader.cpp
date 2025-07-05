#include "ShaderUtil/IRHIShader.h"
#include "ShaderUtil/glTFShaderUtils.h"

IRHIShader::IRHIShader(RHIShaderType type)
    :m_type(type)
{
}

RHIShaderType IRHIShader::GetType() const
{
    return m_type;
}

const std::string& IRHIShader::GetShaderContent() const
{
    return m_shader_content;
}

const std::string& IRHIShader::GetMainEntry() const
{
    return m_shader_entry_function_name;
}

void IRHIShader::SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros)
{
    m_macros = macros;
}

bool IRHIShader::InitShader(const std::string& shader_file_path, RHIShaderType type, const std::string& entry_function_name)
{
    GLTF_CHECK(m_type == RHIShaderType::Unknown);
    
    m_type = type;
    m_shader_file_path = shader_file_path;
    m_shader_entry_function_name = entry_function_name;
    
    return true;
}

bool IRHIShader::CompileShader()
{
    m_macros.AddMacro(GetShaderTypeName(), "1");
    
    glTFShaderUtils::ShaderCompileDesc compile_desc{};
    compile_desc.file_path = m_shader_file_path;
    compile_desc.entry_function = m_shader_entry_function_name;
    compile_desc.shader_macros = m_macros;
    compile_desc.compile_target = glTFShaderUtils::GetShaderCompileTarget(m_type);
    compile_desc.spirv = CompileSpirV();
    
    return glTFShaderUtils::CompileShader(compile_desc, m_shader_byte_code);
}

const std::vector<unsigned char>& IRHIShader::GetShaderByteCode() const
{
    return m_shader_byte_code;
}

std::string IRHIShader::GetShaderTypeName() const
{
    switch (m_type)
    {
    case RHIShaderType::Vertex:
        return "VERTEX_SHADER";
        break;
    case RHIShaderType::Pixel:
        return "PIXEL_SHADER";
        break;
    case RHIShaderType::Compute:
        return "COMPUTE_SHADER";
        break;
    case RHIShaderType::RayTracing:
        return "RAY_TRACING_SHADER";
        break;
    case RHIShaderType::Unknown:
        GLTF_CHECK(false);        
        break;
    }
    GLTF_CHECK(false);
    return "";
}

bool IRHIShader::LoadShader(const std::string& shader_file_path)
{
    m_shaderFilePath = shader_file_path;
    return glTFShaderUtils::LoadShaderFile(shader_file_path.c_str(), m_shader_content);
}
