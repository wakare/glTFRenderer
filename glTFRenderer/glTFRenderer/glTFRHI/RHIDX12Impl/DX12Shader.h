#pragma once
#include <d3dcommon.h>
#include <vector>

#include "../RHIInterface/IRHIShader.h"

class DX12Shader : public IRHIShader
{
public:
    DX12Shader();
    
    bool CompileShaderByteCode(const std::vector<D3D_SHADER_MACRO>& macro);
    const std::vector<unsigned char>& GetShaderByteCode() const;  
    
    virtual bool InitShader(const std::string& shaderFilePath, RHIShaderType type) override;
    
private:
    std::vector<unsigned char> m_shaderByteCode;
};
