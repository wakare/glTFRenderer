#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderSystem/VT/VTTextureTypes.h"

enum class InterfaceVTType
{
    SAMPLE_VT_TEXTURE_DATA,
    RENDER_VT_FEEDBACK,
};

struct VTSystemInfo
{
    inline static std::string Name = "VT_SYSTEM_REGISTER_CBV_INDEX"; 

    unsigned page_size;
    unsigned border_size;
    unsigned physical_texture_width;
    unsigned physical_texture_height;
};

class glTFRenderInterfaceVT : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceVT(InterfaceVTType feed_back);
    
    virtual bool PreInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
protected:
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) override;

    InterfaceVTType m_type;
    
    std::shared_ptr<IRHITexture> m_physical_texture;
    std::vector<std::shared_ptr<IRHITexture>> m_feedback_textures;
    std::vector<VTLogicalTextureInfo> m_vt_logical_texture_infos;
};