#include "IRHIPipelineStateObject.h"

IRHIPipelineStateObject::IRHIPipelineStateObject(RHIPipelineType type)
    : m_type(type)
    , m_cullMode(IRHICullMode::NONE)
{
}

bool IRHIPipelineStateObject::BindInputLayoutAndSetShaderMacros(const std::vector<RHIPipelineInputLayout>& input_layouts)
{
    RETURN_IF_FALSE(!input_layouts.empty())
    
    m_input_layouts = input_layouts;

    // Add shader pre define macros
    for (const auto& input_layout : m_input_layouts)
    {
        if (input_layout.semanticName == INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL))
        {
            m_shaderMacros.AddMacro("HAS_NORMAL", "1");
        }

        if (input_layout.semanticName == INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD))
        {
            m_shaderMacros.AddMacro("HAS_TEXCOORD", "1");
        }

        if (input_layout.semanticName == INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT))
        {
            m_shaderMacros.AddMacro("HAS_TANGENT", "1");
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
