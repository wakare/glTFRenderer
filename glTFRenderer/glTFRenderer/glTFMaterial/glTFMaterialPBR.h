#pragma once
#include "glTFMaterialOpaque.h"

class glTFMaterialPBR : public glTFMaterialOpaque
{
public:
    bool HasBaseColorTexture() const;
    bool HasNormalTexture() const;
    bool HasMetallicRoughnessTexture() const;
};
