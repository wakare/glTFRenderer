#include "IRHIShader.h"

#include "../../glTFShaders/glTFShaderUtils.h"

IRHIShader::IRHIShader(RHIShaderType type)
    :m_type(type)
{
}

RHIShaderType IRHIShader::GetType() const
{
    return m_type;
}

const std::wstring& IRHIShader::GetShaderContent() const
{
    return m_shaderContent;
}

void IRHIShader::SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros)
{
    m_macros = macros;
}

bool IRHIShader::LoadShader(const std::string& shaderFilePath)
{
    return glTFShaderUtils::LoadShaderFile(shaderFilePath.c_str(), m_shaderContent);
}
