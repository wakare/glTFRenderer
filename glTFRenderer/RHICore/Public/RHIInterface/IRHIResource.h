#pragma once

#include "RHICommon.h"

class glTFRenderResourceManager;
class IRHIMemoryManager;

class RHICORE_API IRHIResource
{
    friend class RHIResourceFactory;
    
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIResource)

    const std::string& GetName() const;
    void SetName(const std::string& name);

    virtual bool Release(IRHIMemoryManager& memory_manager) = 0;
    
    void SetNeedRelease();
    bool IsNeedRelease() const;
    
protected:
    std::string m_resource_name;

    bool need_release = false;
};
