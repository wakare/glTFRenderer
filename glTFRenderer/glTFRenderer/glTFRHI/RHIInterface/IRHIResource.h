#pragma once

#include "RHICommon.h"

class glTFRenderResourceManager;

class IRHIResource
{
    friend class RHIResourceFactory;
    
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIResource)

    const std::string& GetName() const { return m_resource_name; }
    void SetName(const std::string& name) {m_resource_name = name; }

    virtual bool Release(glTFRenderResourceManager&) = 0;
    
    void SetNeedRelease() {need_release = true;}
    bool IsNeedRelease() const { return need_release; }
    
protected:
    std::string m_resource_name;

    bool need_release = false;
};
