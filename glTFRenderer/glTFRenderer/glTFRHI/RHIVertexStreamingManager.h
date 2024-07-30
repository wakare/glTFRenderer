#pragma once
#include <vector>

#include "glTFScene/glTFScenePrimitive.h"
#include "RHIInterface/RHICommon.h"

struct MeshVertexInputLayout
{
    VertexLayoutDeclaration m_source_vertex_layout;

    MeshVertexInputLayout();
    bool ResolveInputVertexLayout(std::vector<RHIPipelineInputLayout>& m_vertex_input_layouts) const;
};

struct MeshInstanceInputLayout
{
    VertexLayoutDeclaration m_instance_layout;
    
    MeshInstanceInputLayout();
    bool ResolveInputInstanceLayout(std::vector<RHIPipelineInputLayout>& out_layout) const;
};

class RHIVertexStreamingManager
{
public:
    bool Init(const VertexLayoutDeclaration& vertex_layout, bool has_instance = true);
    void ConfigShaderMacros(RHIShaderPreDefineMacros& shader_macros) const;

    const MeshInstanceInputLayout& GetInstanceInputLayout() const;
    const std::vector<RHIPipelineInputLayout>& GetVertexAttributes() const;
    
protected:
    std::vector<RHIPipelineInputLayout> m_vertex_attributes;
    
    MeshVertexInputLayout m_vertex_input_layout;
    MeshInstanceInputLayout m_instance_input_layout;
};