#pragma once
#include <d3dcommon.h>
#include <vector>

#include "../RHIInterface/IRHIShader.h"

class DX12Shader : public IRHIShader
{
public:
    DX12Shader();
    virtual ~DX12Shader() override;
    
    
    const std::vector<unsigned char>& GetShaderByteCode() const;  
    
    virtual bool InitShader(const std::string& shader_file_path, RHIShaderType type, const std::string& entry_function_name) override;
    virtual bool CompileShader() override;
    
private:
    bool CompileShaderWithFXC();
    bool CompileShaderWithDXC();
    
    const char* GetShaderCompilerTarget() const;
    
    std::vector<unsigned char> m_shader_byte_code;
};
