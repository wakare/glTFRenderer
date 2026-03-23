#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/glm/glm.hpp>

#include "RendererCommon.h"
#include "RendererInterface.h"

// ------- RendererModuleLightmap.hlsl -------
static unsigned LIGHTMAP_ATLAS_INVALID_INDEX = 0xffffffff;
struct LightmapBindingShaderInfo
{
    unsigned atlas_index{0};
    unsigned flags{0};
    glm::fvec4 scale_bias{1.0f, 1.0f, 0.0f, 0.0f};
};
// ------- RendererModuleLightmap.hlsl -------

class RendererModuleLightmap : public RendererInterface::RendererModuleBase
{
public:
    RendererModuleLightmap(RendererInterface::ResourceOperator& resource_operator, const std::string& scene_file);

    unsigned ResolveBindingIndex(unsigned stable_node_key, unsigned primitive_hash) const;

    virtual bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator) override;
    virtual bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) override;

    bool HasLightmapPackage() const { return m_has_lightmap_package; }
    const std::string& GetLoadStatus() const { return m_load_status; }

protected:
    struct AtlasSourceDesc
    {
        unsigned atlas_id{0};
        std::filesystem::path file_path{};
        unsigned width{0};
        unsigned height{0};
        std::string format{};
        std::string codec{"raw_half"};
        std::string semantic{};
    };

    struct BindingEntry
    {
        unsigned atlas_id{0};
        unsigned primitive_hash{RendererUniqueObjectIDInvalid};
        unsigned stable_node_key{RendererUniqueObjectIDInvalid};
        unsigned runtime_binding_index{0};
        glm::fvec4 scale_bias{1.0f, 1.0f, 0.0f, 0.0f};
    };

    bool LoadManifest();
    RendererInterface::TextureHandle CreateFallbackTexture();

    static unsigned long long MakeInstancePrimitiveKey(unsigned stable_node_key, unsigned primitive_hash);

    RendererInterface::ResourceOperator& m_resource_operator;
    std::string m_scene_file{};
    std::filesystem::path m_manifest_path{};

    std::vector<AtlasSourceDesc> m_atlas_sources{};
    std::vector<BindingEntry> m_binding_entries{};
    std::map<unsigned, unsigned> m_binding_index_by_primitive_hash{};
    std::map<unsigned long long, unsigned> m_binding_index_by_instance_primitive{};

    std::vector<RendererInterface::TextureHandle> m_lightmap_texture_handles{};
    RendererInterface::BufferDesc m_lightmap_binding_buffer_desc{};
    RendererInterface::BufferHandle m_lightmap_binding_buffer_handle{NULL_HANDLE};
    unsigned m_lightmap_binding_count{0};
    bool m_has_lightmap_package{false};
    std::string m_load_status{};
};
