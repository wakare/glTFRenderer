#pragma once
#include "glTFRenderInterfaceBase.h"

// Use material manager class to bind all needed texture in bindless mode
class glTFRenderInterfaceMaterialManager : public glTFRenderInterfaceBase
{
public:
    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override;
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override;
};
