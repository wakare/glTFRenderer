#pragma once
#include <d3dcommon.h>
#include <vector>

#include "../RHIInterface/IRHIShader.h"

class DX12Shader : public IRHIShader
{
public:
    DX12Shader();
    virtual ~DX12Shader() override;
    
    bool CompileShaderByteCode();
    const std::vector<unsigned char>& GetShaderByteCode() const;  
    
    virtual bool InitShader(const std::string& shaderFilePath, RHIShaderType type, const std::string& entryFunctionName,
        RayTracingShaderEntryFunctionNames raytracing_entry_names) override;
    virtual bool CompileShader() override;
    
private:
    const char* GetShaderCompilerTarget() const;
    
    std::vector<unsigned char> m_shaderByteCode;
};
