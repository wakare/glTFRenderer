#pragma once

#include "RHICommon.h"

class IRHIResource
{
    friend class RHIResourceFactory;
    
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIResource)

    const std::string& GetName() const { return m_resource_name; }
    void SetName(const std::string& name) {m_resource_name = name; }
    
protected:
    std::string m_resource_name;
};
