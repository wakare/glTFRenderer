#pragma once
#include <string>
#include "IRHIResource.h"

enum class RHIShaderType
{
    Vertex,
    Pixel,
    Unknown,
};

class IRHIShader : public IRHIResource
{
public:
    IRHIShader(RHIShaderType type = RHIShaderType::Unknown);
    RHIShaderType GetType() const;
    const std::string GetShaderContent() const;
    
    virtual bool InitShader(const std::string& shaderFilePath, RHIShaderType type) = 0;

protected:
    bool LoadShader(const std::string& shaderFilePath);
    
    RHIShaderType m_type;
    std::string m_shaderContent;
};
