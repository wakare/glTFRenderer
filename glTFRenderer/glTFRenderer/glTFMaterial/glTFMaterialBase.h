#pragma once
#include "../glTFUtils/glTFUtils.h"

enum class MaterialType
{
    Opaque,
    Unknown,
};

class glTFMaterialBase : public glTFUniqueObject
{
public:
    glTFMaterialBase(MaterialType type) : m_type(type) {}
    
    const MaterialType& GetMaterialType() const {return m_type; }

private:
    MaterialType m_type;
};