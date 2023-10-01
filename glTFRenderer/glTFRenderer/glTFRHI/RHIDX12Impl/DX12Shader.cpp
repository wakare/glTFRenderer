#include "DX12Shader.h"

#include <cassert>
#include <d3dcompiler.h>
#include "glTFUtils/glTFUtils.h"

DX12Shader::DX12Shader()
    : IRHIShader()
{
}

DX12Shader::~DX12Shader()
{
}

bool DX12Shader::CompileShaderByteCode()
{
    if (m_shaderContent.empty() )
    {
        // Not init shader content!
        assert(false);
        return false;
    }

    if (m_type != RHIShaderType::RayTracing && m_shaderEntryFunctionName.empty())
    {
        GLTF_CHECK(false);
    }
    
    assert(m_macros.macroKey.size() == m_macros.macroValue.size());
    std::vector<D3D_SHADER_MACRO> dxShaderMacros;
    if (!m_macros.macroKey.empty())
    {
        for (size_t i = 0; i < m_macros.macroKey.size(); i++)
        {
            const std::string& key = m_macros.macroKey[i];
            const std::string& value = m_macros.macroValue[i];
            
            dxShaderMacros.push_back({key.c_str(), value.c_str()});
            LOG_FORMAT_FLUSH("[DEBUG] Compile with macro %s = %s\n", key.c_str(), value.c_str())
        }
        dxShaderMacros.push_back({nullptr, nullptr});
    }
    
    ID3DBlob* shaderCompileResult = nullptr; // d3d blob for holding vertex shader bytecode
    ID3DBlob* errorBuff = nullptr; // a buffer holding the error data if any
    
    D3DCompile(m_shaderContent.data(),
            m_shaderContent.length(),
            nullptr,
            dxShaderMacros.data(),
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            m_shaderEntryFunctionName.c_str(),
            GetShaderCompilerTarget(), 
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            &shaderCompileResult,
            &errorBuff);

    if (errorBuff)
    {
        LOG_FORMAT_FLUSH("[FATAL] Compile shader file %s failed\nresult: %s\n", m_shaderFilePath.c_str(), static_cast<const char*>(errorBuff->GetBufferPointer()));
        return false;
    }
    
    // store shader bytecode
    m_shaderByteCode.resize(shaderCompileResult->GetBufferSize());
    memcpy(m_shaderByteCode.data(), shaderCompileResult->GetBufferPointer(), shaderCompileResult->GetBufferSize());
    
    return true;
}

const std::vector<unsigned char>& DX12Shader::GetShaderByteCode() const
{
    return m_shaderByteCode;
}

bool DX12Shader::InitShader(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName,
    RayTracingShaderEntryFunctionNames raytracing_entry_names)
{
    assert(m_type == RHIShaderType::Unknown);
    m_type = type;
    m_shaderEntryFunctionName = entryFunctionName;
    m_raytracing_entry_names = raytracing_entry_names;
    
    if (!LoadShader(shaderFilePath))
    {
        return false;
    }
    
    return true;
}

bool DX12Shader::CompileShader()
{
    return CompileShaderByteCode();
}

const char* DX12Shader::GetShaderCompilerTarget() const
{
    // 5.0 is latest target for dx11 version
    switch (m_type) {
        case RHIShaderType::Vertex: return "vs_5_0";
        case RHIShaderType::Pixel: return "ps_5_0";
        case RHIShaderType::Compute: return "cs_5_0";

        // raytracing shader will be compile into lib
        case RHIShaderType::RayTracing: return "lib_6_3";
    }

    assert(false);
    return "UnknownTarget";
}
