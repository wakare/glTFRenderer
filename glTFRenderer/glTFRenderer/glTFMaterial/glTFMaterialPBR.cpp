#include "glTFMaterialPBR.h"

bool glTFMaterialPBR::HasBaseColorTexture() const
{
    return HasValidParameterTexture(glTFMaterialParameterUsage::BASECOLOR);
}

bool glTFMaterialPBR::HasNormalTexture() const
{
    return HasValidParameterTexture(glTFMaterialParameterUsage::NORMAL);
}

bool glTFMaterialPBR::HasMetallicRoughnessTexture() const
{
    return HasValidParameterTexture(glTFMaterialParameterUsage::METALLIC_ROUGHNESS);
}