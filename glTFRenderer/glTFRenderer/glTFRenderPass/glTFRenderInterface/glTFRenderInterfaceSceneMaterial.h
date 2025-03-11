#pragma once
#include "glTFRenderInterfaceBase.h"

static unsigned material_texture_invalid_index = UINT_MAX;
ALIGN_FOR_CBV_STRUCT struct MaterialInfo
{
    enum
    {
        VT_FLAG_ALBEDO = 0x1,
        VT_FLAG_NORMAL = 0x2,
        VT_FLAG_SPECULAR = 0x4,
    };
    
    inline static std::string Name = "SCENE_MATERIAL_INFO_REGISTER_INDEX";
    
    unsigned albedo_tex_index {material_texture_invalid_index};
    unsigned normal_tex_index {material_texture_invalid_index};
    unsigned metallic_roughness_tex_index {material_texture_invalid_index};
    unsigned vt_flags;
    
    glm::vec4 albedo {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 normal {0.0f, 0.0f, 1.0f, 0.0f};
    glm::vec4 metallicAndRoughness {0.0f, 1.0f, 0.0f, 0.0f};
};

struct SceneMaterialInterfaceConfig
{
    bool vt_feed_back;
};

// Use material manager class to bind all needed textures in bindless mode
class glTFRenderInterfaceSceneMaterial : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceSceneMaterial(const SceneMaterialInterfaceConfig& config);

    bool UploadMaterialData(glTFRenderResourceManager& resource_manager);
    
protected:
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
};
