#include "glTFMaterialPBR.h"

bool glTFMaterialPBR::HasBaseColor() const
{
    return HasValidParameterTexture(glTFMaterialParameterUsage::BASECOLOR);
}

bool glTFMaterialPBR::HasNormal() const
{
    return HasValidParameterTexture(glTFMaterialParameterUsage::NORMAL);
}

bool glTFMaterialPBR::HasMetallic() const
{
    return HasValidParameterTexture(glTFMaterialParameterUsage::METALNESS);
}

bool glTFMaterialPBR::HasRoughness() const
{
    return HasValidParameterTexture(glTFMaterialParameterUsage::ROUGHNESS);
}
