#pragma once
#include "glTFMaterialCommon.h"
#include "RendererCommon.h"

// Material parameter may be factor or texture
class glTFMaterialParameterBase : public RendererUniqueObjectIDBase<glTFMaterialParameterBase>
{
public:
    glTFMaterialParameterBase(glTFMaterialParameterType type, glTFMaterialParameterUsage usage);

    glTFMaterialParameterType GetParameterType() const {return m_parameter_type;}
	glTFMaterialParameterUsage GetParameterUsage() const {return m_parameter_usage;}
    
protected:
    glTFMaterialParameterType m_parameter_type;
    glTFMaterialParameterUsage m_parameter_usage;
};