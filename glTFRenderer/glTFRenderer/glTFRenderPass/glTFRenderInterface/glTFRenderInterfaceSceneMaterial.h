#pragma once
#include "glTFRenderInterfaceBase.h"

struct MaterialInfo
{
    inline static std::string Name = "SCENE_MATERIAL_INFO_REGISTER_INDEX";
    
    unsigned albedo_tex_index;
    unsigned normal_tex_index;
};

// Use material manager class to bind all needed textures in bindless mode
class glTFRenderInterfaceSceneMaterial : public glTFRenderInterfaceBaseWithDefaultImpl
{
public:
    glTFRenderInterfaceSceneMaterial();
};
