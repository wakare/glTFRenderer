#include "glTFElementCommon.h"

std::string AttributeName(glTF_Attribute attribute)
{
#define RETURN_ATTRIBUTE_NAME(ATTRIBUTE_NAME) case E##ATTRIBUTE_NAME: return #ATTRIBUTE_NAME;
    
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

