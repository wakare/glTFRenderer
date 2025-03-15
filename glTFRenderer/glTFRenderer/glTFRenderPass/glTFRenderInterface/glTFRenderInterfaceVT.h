#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderSystem/VT/VTTextureTypes.h"

class glTFRenderInterfaceVT : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceVT(bool feed_back);
    
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
protected:
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override;
    
    bool m_feed_back;
    std::shared_ptr<IRHITexture> m_physical_texture;
    std::vector<std::shared_ptr<IRHITexture>> m_feedback_textures;
};