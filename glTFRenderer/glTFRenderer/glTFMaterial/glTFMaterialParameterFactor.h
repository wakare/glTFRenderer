#pragma once
#include "glTFMaterialParameterBase.h"

template<typename FactorType>
class glTFMaterialParameterFactor : public glTFMaterialParameterBase
{
public:
    glTFMaterialParameterFactor(glTFMaterialParameterUsage usage, FactorType factor);
    FactorType GetFactor() const;
    
protected:
    FactorType m_factor;
};

template <typename FactorType>
glTFMaterialParameterFactor<FactorType>::glTFMaterialParameterFactor(glTFMaterialParameterUsage usage, FactorType factor)
    : glTFMaterialParameterBase(Factor, usage)
	, m_factor(factor)
{
}

template <typename FactorType>
FactorType glTFMaterialParameterFactor<FactorType>::GetFactor() const
{
    return m_factor;
}
