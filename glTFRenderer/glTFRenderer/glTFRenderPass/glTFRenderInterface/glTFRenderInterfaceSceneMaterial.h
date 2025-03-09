#pragma once
#include "glTFRenderInterfaceBase.h"

static unsigned material_texture_invalid_index = UINT_MAX;
ALIGN_FOR_CBV_STRUCT struct MaterialInfo
{
    inline static std::string Name = "SCENE_MATERIAL_INFO_REGISTER_INDEX";
    
    unsigned albedo_tex_index {material_texture_invalid_index};
    unsigned normal_tex_index {material_texture_invalid_index};
    unsigned metallic_roughness_tex_index {material_texture_invalid_index};
    unsigned pad;
    
    glm::vec4 albedo {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 normal {0.0f, 0.0f, 1.0f, 0.0f};
    glm::vec4 metallicAndRoughness {0.0f, 1.0f, 0.0f, 0.0f};
};

// Use material manager class to bind all needed textures in bindless mode
class glTFRenderInterfaceSceneMaterial : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceSceneMaterial();

    bool UploadMaterialData(glTFRenderResourceManager& resource_manager);
    
protected:
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
};
