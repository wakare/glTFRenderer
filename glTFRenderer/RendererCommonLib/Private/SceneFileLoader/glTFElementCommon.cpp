#include "SceneFileLoader/glTFElementCommon.h"

void glTF_Transform::SetMatrixData(const float* data)
{
    memcpy(&m_matrix, data, sizeof(m_matrix));
}

const glm::mat4& glTF_Transform::GetMatrix() const
{
    return m_matrix;
}

std::string AttributeName(glTF_Attribute_Base::glTF_Attribute attribute)
{
#define RETURN_ATTRIBUTE_NAME(ATTRIBUTE_NAME) case glTF_Attribute_Base::E##ATTRIBUTE_NAME: return #ATTRIBUTE_NAME;
    
    switch (attribute)
    {
        RETURN_ATTRIBUTE_NAME(Position)
        RETURN_ATTRIBUTE_NAME(Normal)
        RETURN_ATTRIBUTE_NAME(Tangent)
        RETURN_ATTRIBUTE_NAME(Color)
        RETURN_ATTRIBUTE_NAME(TexCoord)
        RETURN_ATTRIBUTE_NAME(Joint)
        RETURN_ATTRIBUTE_NAME(Weight)
        RETURN_ATTRIBUTE_NAME(Unknown)
        }
    GLTF_CHECK(false);
    return "Unknown";
}

