#pragma once
#include <string>
#include <vector>

#include "IRHIResource.h"

enum class RHIShaderType
{
    Vertex,
    Pixel,
    Unknown,
};

struct RHIShaderPreDefineMacros
{
    std::vector<std::string> macroKey;
    std::vector<std::string> macroValue;
};

class IRHIShader : public IRHIResource
{
public:
    IRHIShader(RHIShaderType type = RHIShaderType::Unknown);
    RHIShaderType GetType() const;
    const std::wstring& GetShaderContent() const;
    void SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros);
    
    virtual bool InitShader(const std::string& shaderFilePath, RHIShaderType type) = 0;
    virtual bool CompileShader() = 0;
    
protected:
    bool LoadShader(const std::string& shaderFilePath);
    
    RHIShaderType m_type;
    std::wstring m_shaderContent;

    RHIShaderPreDefineMacros m_macros;
};
