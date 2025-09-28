#pragma once
#include "RendererCommon.h"

enum class MaterialType
{
    Opaque,
    Translucent,
    Unknown,
};

class glTFMaterialBase : public RendererUniqueObjectIDBase<glTFMaterialBase>
{
public:
    glTFMaterialBase(MaterialType type) : m_type(type) {}
    
    const MaterialType& GetMaterialType() const {return m_type; }

private:
    MaterialType m_type;
};
