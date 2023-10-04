#pragma once
#include "glTFRenderInterfaceBase.h"

struct MaterialInfo
{
    inline static std::string Name = "SCENE_MATERIAL_INFO_REGISTER_INDEX";
    
    unsigned albedo_tex_index;
    unsigned normal_tex_index;
};

// Use material manager class to bind all needed textures in bindless mode
class glTFRenderInterfaceSceneMaterial : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceSceneMaterial();
    
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override;
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override;
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override;
};
