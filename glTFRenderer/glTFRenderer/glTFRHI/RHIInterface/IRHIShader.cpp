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

const std::string IRHIShader::GetShaderContent() const
{
    return m_shaderContent;
}

bool IRHIShader::LoadShader(const std::string& shaderFilePath)
{
    return glTFShaderUtils::LoadShaderFile(shaderFilePath.c_str(), m_shaderContent);
}
