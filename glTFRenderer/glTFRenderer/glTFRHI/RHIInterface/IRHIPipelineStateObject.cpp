#include "IRHIPipelineStateObject.h"

IRHIPipelineStateObject::IRHIPipelineStateObject(RHIPipelineType type)
    : m_type(type)
    , m_cullMode(IRHICullMode::NONE)
{
}

bool IRHIPipelineStateObject::BindInputLayout(const std::vector<RHIPipelineInputLayout>& inputLayout)
{
    RETURN_IF_FALSE(!inputLayout.empty())
    
    m_inputLayout = inputLayout;

    // Add shader pre define macros
    for (const auto& inputLayout : m_inputLayout)
    {
        if (inputLayout.semanticName == g_inputLayoutNameNORMAL)
        {
            m_shaderMacros.AddMacro("HAS_NORMAL", "1");
        }

        if (inputLayout.semanticName == g_inputLayoutNameTEXCOORD)
        {
            m_shaderMacros.AddMacro("HAS_TEXCOORD", "1");
        }
    }
    
    return true;
}

RHIShaderPreDefineMacros& IRHIPipelineStateObject::GetShaderMacros()
{
    return m_shaderMacros;
}

void IRHIPipelineStateObject::SetCullMode(IRHICullMode mode)
{
    m_cullMode = mode;
}

IRHICullMode IRHIPipelineStateObject::GetCullMode() const
{
    return m_cullMode;
}
