#pragma once
#include "../glTFUtils/glTFUtils.h"

enum glTFMaterialParameterType
{
    Factor,
    Texture,
};

// Material parameter may be factor or texture
class glTFMaterialParameterBase : public glTFUniqueObject
{
public:
    glTFMaterialParameterBase(glTFMaterialParameterType type)
        : m_parameter_type(type)
    {
        
    }

    
    glTFMaterialParameterType GetParameterType() const {return m_parameter_type;}

protected:
    glTFMaterialParameterType m_parameter_type;
};
