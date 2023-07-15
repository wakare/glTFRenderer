#pragma once
#include "glTFMaterialOpaque.h"

class glTFMaterialPBR : public glTFMaterialOpaque
{
public:
    bool HasBaseColor() const;
    bool HasNormal() const;
    bool HasMetallic() const;
    bool HasRoughness() const;


};
