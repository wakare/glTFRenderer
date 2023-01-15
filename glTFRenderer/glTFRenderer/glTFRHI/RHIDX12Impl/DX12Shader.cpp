#include "DX12Shader.h"

#include <cassert>

DX12Shader::DX12Shader()
    : IRHIShader()
{
}

bool DX12Shader::CompileShaderByteCode(const std::vector<D3D_SHADER_MACRO>& macro)
{

    
    return true;
}

const std::vector<unsigned char>& DX12Shader::GetShaderByteCode() const
{
    return m_shaderByteCode;
}

bool DX12Shader::InitShader(const std::string& shaderFilePath, RHIShaderType type)
{
    assert(m_type == RHIShaderType::Unknown);
    m_type = type;
    
    if (!LoadShader(shaderFilePath))
    {
        return false;
    }
    
    return true;
}
