#pragma once
#include <vector>

#include "glTFScene/glTFScenePrimitive.h"
#include "glTFRHI/RHIInterface/RHICommon.h"

class RHIVertexStreamingManager
{
public:
    virtual bool Init(const VertexLayoutDeclaration& vertex_layout, bool has_instance = true);
    void ConfigShaderMacros(RHIShaderPreDefineMacros& shader_macros) const;

    std::vector<VertexAttributeElement> GetInstanceInputLayout() const;
    const std::vector<RHIPipelineInputLayout>& GetVertexAttributes() const;
    
protected:
    std::vector<RHIPipelineInputLayout> m_vertex_attributes;
};