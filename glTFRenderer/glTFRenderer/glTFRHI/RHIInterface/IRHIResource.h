#pragma once

#include "RHICommon.h"

class IRHIResource
{
    friend class RHIResourceFactory;
public:
    IRHIResource() = default;

    // Clean resource in virtual dtor!!
    virtual ~IRHIResource() = 0;

    const std::string& GetName() const { return m_resourceName; }
    void SetName(const std::string& name) {m_resourceName = name; }
    
private:
    // Disable copy and move ctor and assignment operation
    IRHIResource(const IRHIResource&) = default;
    IRHIResource& operator=(const IRHIResource&) = default;

    IRHIResource(IRHIResource&&) = default;
    IRHIResource& operator=(IRHIResource&&) = default;

    std::string m_resourceName;
};
