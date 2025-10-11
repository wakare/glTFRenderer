#pragma once
#include <map>
#include <glm/glm/glm.hpp>

#include "RendererCommon.h"
#include "SceneRendererUtil/SceneRendererCameraOperator.h"

class MaterialBase;

// ------- RendererModuleMaterial.hlsl -------
static unsigned MATERIAL_TEXTURE_INVALID_INDEX = 0xffffffff;
struct MaterialShaderInfo
{
    unsigned albedo_tex_index {MATERIAL_TEXTURE_INVALID_INDEX};
    unsigned normal_tex_index {MATERIAL_TEXTURE_INVALID_INDEX};
    unsigned metallic_roughness_tex_index {MATERIAL_TEXTURE_INVALID_INDEX};
    unsigned virtual_texture_flag;
    
    glm::fvec4 albedo;
    glm::fvec4 normal;
    glm::fvec4 metallic_and_roughness;
};
// ------- RendererModuleMaterial.hlsl -------

class RendererModuleMaterial
{
public:
    RendererModuleMaterial(RendererInterface::ResourceOperator& resource_operator);

    bool AddMaterial(const MaterialBase& material);
    bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator);
    bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc);
    
protected:
    RendererInterface::ResourceOperator& m_resource_operator;
    
    std::map<unsigned, MaterialShaderInfo> m_material_shader_infos;
    std::vector<std::string> m_material_texture_uris;
    std::vector<RendererInterface::TextureHandle> m_material_texture_handles;

    RendererInterface::BufferDesc m_material_shader_info_buffer_desc{};
    RendererInterface::BufferHandle m_material_infos_handle;
};
