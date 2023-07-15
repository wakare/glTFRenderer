#include "glTFMaterialPBR.h"

bool glTFMaterialPBR::HasBaseColor() const
{
    return HasValidParameter(glTFMaterialParameterUsage::BASECOLOR);
}

bool glTFMaterialPBR::HasNormal() const
{
    return HasValidParameter(glTFMaterialParameterUsage::NORMAL);
}

bool glTFMaterialPBR::HasMetallic() const
{
    return HasValidParameter(glTFMaterialParameterUsage::METALNESS);
}

bool glTFMaterialPBR::HasRoughness() const
{
    return HasValidParameter(glTFMaterialParameterUsage::ROUGHNESS);
}
