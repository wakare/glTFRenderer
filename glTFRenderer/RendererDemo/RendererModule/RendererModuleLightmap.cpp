#include "RendererModuleLightmap.h"

#include <fstream>
#include <sstream>

#include <nlohmann_json/single_include/nlohmann/json.hpp>

namespace
{
    std::filesystem::path ResolveLightmapManifestPath(const std::string& scene_file)
    {
        const std::filesystem::path scene_path(scene_file);
        const std::filesystem::path stem_sidecar =
            scene_path.parent_path() / (scene_path.stem().string() + ".lmbake") / "manifest.json";
        if (std::filesystem::exists(stem_sidecar))
        {
            return stem_sidecar;
        }

        const std::filesystem::path suffix_sidecar = std::filesystem::path(scene_file + ".lmbake") / "manifest.json";
        if (std::filesystem::exists(suffix_sidecar))
        {
            return suffix_sidecar;
        }

        return stem_sidecar;
    }

    bool ReadUnsignedField(const nlohmann::json& object, const char* key, unsigned& out_value, std::string& out_error)
    {
        if (!object.contains(key))
        {
            out_error = std::string("Missing required unsigned field: ") + key;
            return false;
        }
        if (!object.at(key).is_number_unsigned())
        {
            out_error = std::string("Field must be unsigned integer: ") + key;
            return false;
        }

        out_value = object.at(key).get<unsigned>();
        return true;
    }

    bool ReadStringField(const nlohmann::json& object, const char* key, std::string& out_value, std::string& out_error)
    {
        if (!object.contains(key))
        {
            out_error = std::string("Missing required string field: ") + key;
            return false;
        }
        if (!object.at(key).is_string())
        {
            out_error = std::string("Field must be string: ") + key;
            return false;
        }

        out_value = object.at(key).get<std::string>();
        return true;
    }

    bool ReadOptionalScaleBias(const nlohmann::json& object, const char* key, glm::fvec4& out_value, std::string& out_error)
    {
        if (!object.contains(key))
        {
            return true;
        }
        const auto& value = object.at(key);
        if (!value.is_array() || value.size() != 4)
        {
            out_error = std::string("Field must be float[4]: ") + key;
            return false;
        }

        for (int component_index = 0; component_index < 4; ++component_index)
        {
            if (!value.at(component_index).is_number())
            {
                out_error = std::string("Field must be float[4]: ") + key;
                return false;
            }
            out_value[component_index] = value.at(component_index).get<float>();
        }
        return true;
    }

    bool ReadBinaryFile(const std::filesystem::path& file_path, std::vector<char>& out_data, std::string& out_error)
    {
        std::ifstream stream(file_path, std::ios::in | std::ios::binary | std::ios::ate);
        if (!stream.is_open())
        {
            out_error = "Failed to open file: " + file_path.string();
            return false;
        }

        const std::streamsize file_size = stream.tellg();
        if (file_size < 0)
        {
            out_error = "Failed to query file size: " + file_path.string();
            return false;
        }

        out_data.resize(static_cast<size_t>(file_size));
        stream.seekg(0, std::ios::beg);
        if (!out_data.empty())
        {
            stream.read(out_data.data(), file_size);
        }
        if (!stream.good() && !stream.eof())
        {
            out_error = "Failed to read file: " + file_path.string();
            return false;
        }
        return true;
    }
}

RendererModuleLightmap::RendererModuleLightmap(RendererInterface::ResourceOperator& resource_operator, const std::string& scene_file)
    : m_resource_operator(resource_operator)
    , m_scene_file(scene_file)
{
    LoadManifest();
}

unsigned RendererModuleLightmap::ResolveBindingIndex(unsigned stable_node_key, unsigned primitive_hash) const
{
    const unsigned long long instance_primitive_key = MakeInstancePrimitiveKey(stable_node_key, primitive_hash);
    const auto instance_it = m_binding_index_by_instance_primitive.find(instance_primitive_key);
    if (instance_it != m_binding_index_by_instance_primitive.end())
    {
        return instance_it->second;
    }

    const auto primitive_it = m_binding_index_by_primitive_hash.find(primitive_hash);
    if (primitive_it != m_binding_index_by_primitive_hash.end())
    {
        return primitive_it->second;
    }

    return 0;
}

bool RendererModuleLightmap::FinalizeModule(RendererInterface::ResourceOperator& resource_operator)
{
    (void)resource_operator;

    m_lightmap_texture_handles.clear();
    m_lightmap_texture_handles.push_back(CreateFallbackTexture());

    std::map<unsigned, unsigned> runtime_atlas_indices{};
    for (const auto& atlas_source : m_atlas_sources)
    {
        if (atlas_source.codec != "raw_half" || atlas_source.format != "rgba16f")
        {
            LOG_FORMAT_FLUSH("[Lightmap] Skip unsupported atlas codec/format for %s (codec=%s, format=%s).\n",
                             atlas_source.file_path.string().c_str(),
                             atlas_source.codec.c_str(),
                             atlas_source.format.c_str());
            continue;
        }

        std::vector<char> atlas_data{};
        std::string read_error{};
        if (!ReadBinaryFile(atlas_source.file_path, atlas_data, read_error))
        {
            LOG_FORMAT_FLUSH("[Lightmap] %s\n", read_error.c_str());
            continue;
        }

        const size_t expected_size =
            static_cast<size_t>(atlas_source.width) *
            static_cast<size_t>(atlas_source.height) *
            4u *
            sizeof(uint16_t);
        if (atlas_data.size() != expected_size)
        {
            LOG_FORMAT_FLUSH("[Lightmap] Atlas byte size mismatch for %s. Expected %zu, got %zu.\n",
                             atlas_source.file_path.string().c_str(),
                             expected_size,
                             atlas_data.size());
            continue;
        }

        RendererInterface::TextureDesc texture_desc{};
        texture_desc.format = RendererInterface::RGBA16_FLOAT;
        texture_desc.width = atlas_source.width;
        texture_desc.height = atlas_source.height;
        texture_desc.data = std::move(atlas_data);

        const unsigned runtime_atlas_index = static_cast<unsigned>(m_lightmap_texture_handles.size());
        runtime_atlas_indices[atlas_source.atlas_id] = runtime_atlas_index;
        m_lightmap_texture_handles.push_back(m_resource_operator.CreateTexture(texture_desc));
    }

    std::vector<LightmapBindingShaderInfo> binding_infos(m_binding_entries.size() + 1u);
    binding_infos[0].atlas_index = 0;
    binding_infos[0].flags = 0;
    binding_infos[0].scale_bias = glm::fvec4(1.0f, 1.0f, 0.0f, 0.0f);
    for (const auto& binding_entry : m_binding_entries)
    {
        LightmapBindingShaderInfo shader_info{};
        shader_info.scale_bias = binding_entry.scale_bias;

        const auto atlas_runtime_it = runtime_atlas_indices.find(binding_entry.atlas_id);
        shader_info.atlas_index = atlas_runtime_it != runtime_atlas_indices.end() ? atlas_runtime_it->second : 0u;
        shader_info.flags = shader_info.atlas_index == 0u ? 0u : 0x1u;

        GLTF_CHECK(binding_entry.runtime_binding_index < binding_infos.size());
        binding_infos[binding_entry.runtime_binding_index] = shader_info;
    }

    m_lightmap_binding_count = static_cast<unsigned>(binding_infos.size());
    m_lightmap_binding_buffer_desc.usage = RendererInterface::USAGE_SRV;
    m_lightmap_binding_buffer_desc.type = RendererInterface::DEFAULT;
    m_lightmap_binding_buffer_desc.name = "g_lightmap_bindings";
    m_lightmap_binding_buffer_desc.size = binding_infos.size() * sizeof(LightmapBindingShaderInfo);
    m_lightmap_binding_buffer_desc.data = binding_infos.data();
    m_lightmap_binding_buffer_handle = m_resource_operator.CreateBuffer(m_lightmap_binding_buffer_desc);
    return true;
}

bool RendererModuleLightmap::BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc)
{
    RendererInterface::BufferBindingDesc binding_desc{};
    binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    binding_desc.buffer_handle = m_lightmap_binding_buffer_handle;
    binding_desc.is_structured_buffer = true;
    binding_desc.stride = sizeof(LightmapBindingShaderInfo);
    binding_desc.count = m_lightmap_binding_count;
    out_draw_desc.buffer_resources[m_lightmap_binding_buffer_desc.name] = binding_desc;

    RendererInterface::TextureBindingDesc texture_binding_desc{};
    texture_binding_desc.type = RendererInterface::TextureBindingDesc::SRV;
    texture_binding_desc.textures = m_lightmap_texture_handles;
    out_draw_desc.texture_resources["bindless_lightmaps"] = texture_binding_desc;
    return true;
}

bool RendererModuleLightmap::LoadManifest()
{
    m_manifest_path = ResolveLightmapManifestPath(m_scene_file);
    if (!std::filesystem::exists(m_manifest_path))
    {
        m_load_status = "No sidecar package found.";
        m_has_lightmap_package = false;
        return true;
    }

    std::ifstream stream(m_manifest_path, std::ios::in | std::ios::binary);
    if (!stream.is_open())
    {
        m_load_status = "Failed to open lightmap manifest: " + m_manifest_path.string();
        LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
        return false;
    }

    std::stringstream buffer{};
    buffer << stream.rdbuf();

    nlohmann::json root{};
    try
    {
        root = nlohmann::json::parse(buffer.str());
    }
    catch (const std::exception& exception)
    {
        m_load_status = "Invalid lightmap manifest json: " + std::string(exception.what());
        LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
        return false;
    }

    if (!root.is_object())
    {
        m_load_status = "Lightmap manifest root must be an object.";
        LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
        return false;
    }

    if (!root.contains("atlases") || !root.at("atlases").is_array())
    {
        m_load_status = "Lightmap manifest must contain atlases[].";
        LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
        return false;
    }

    std::string parse_error{};
    for (const auto& atlas_json : root.at("atlases"))
    {
        if (!atlas_json.is_object())
        {
            m_load_status = "Atlas entry must be an object.";
            LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
            return false;
        }

        AtlasSourceDesc atlas_source{};
        std::string atlas_file{};
        if (!ReadUnsignedField(atlas_json, "id", atlas_source.atlas_id, parse_error) ||
            !ReadStringField(atlas_json, "file", atlas_file, parse_error))
        {
            m_load_status = parse_error;
            LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
            return false;
        }

        atlas_source.file_path = m_manifest_path.parent_path() / atlas_file;
        if (!ReadUnsignedField(atlas_json, "width", atlas_source.width, parse_error) ||
            !ReadUnsignedField(atlas_json, "height", atlas_source.height, parse_error))
        {
            m_load_status = parse_error;
            LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
            return false;
        }

        if (!ReadStringField(atlas_json, "format", atlas_source.format, parse_error))
        {
            m_load_status = parse_error;
            LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
            return false;
        }

        if (atlas_json.contains("codec"))
        {
            if (!atlas_json.at("codec").is_string())
            {
                m_load_status = "Atlas codec must be string.";
                LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
                return false;
            }
            atlas_source.codec = atlas_json.at("codec").get<std::string>();
        }

        if (atlas_json.contains("semantic") && atlas_json.at("semantic").is_string())
        {
            atlas_source.semantic = atlas_json.at("semantic").get<std::string>();
        }
        m_atlas_sources.push_back(std::move(atlas_source));
    }

    if (root.contains("bindings"))
    {
        if (!root.at("bindings").is_array())
        {
            m_load_status = "Lightmap bindings must be an array.";
            LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
            return false;
        }

        for (const auto& binding_json : root.at("bindings"))
        {
            if (!binding_json.is_object())
            {
                m_load_status = "Binding entry must be an object.";
                LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
                return false;
            }

            BindingEntry binding_entry{};
            if (!ReadUnsignedField(binding_json, "primitive_hash", binding_entry.primitive_hash, parse_error) ||
                !ReadUnsignedField(binding_json, "atlas_id", binding_entry.atlas_id, parse_error))
            {
                m_load_status = parse_error;
                LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
                return false;
            }

            if (binding_json.contains("node_key"))
            {
                if (!binding_json.at("node_key").is_number_unsigned())
                {
                    m_load_status = "Binding node_key must be unsigned integer.";
                    LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
                    return false;
                }
                binding_entry.stable_node_key = binding_json.at("node_key").get<unsigned>();
            }

            if (!ReadOptionalScaleBias(binding_json, "scale_bias", binding_entry.scale_bias, parse_error))
            {
                m_load_status = parse_error;
                LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
                return false;
            }

            binding_entry.runtime_binding_index = static_cast<unsigned>(m_binding_entries.size() + 1u);
            m_binding_entries.push_back(binding_entry);

            if (binding_entry.stable_node_key != RendererUniqueObjectIDInvalid)
            {
                m_binding_index_by_instance_primitive[MakeInstancePrimitiveKey(binding_entry.stable_node_key, binding_entry.primitive_hash)] =
                    binding_entry.runtime_binding_index;
            }
            else
            {
                m_binding_index_by_primitive_hash[binding_entry.primitive_hash] = binding_entry.runtime_binding_index;
            }
        }
    }

    m_has_lightmap_package = !m_atlas_sources.empty();
    m_load_status = m_has_lightmap_package
        ? ("Loaded lightmap manifest: " + m_manifest_path.string())
        : ("Lightmap manifest contains no atlases: " + m_manifest_path.string());
    LOG_FORMAT_FLUSH("[Lightmap] %s\n", m_load_status.c_str());
    return true;
}

RendererInterface::TextureHandle RendererModuleLightmap::CreateFallbackTexture()
{
    RendererInterface::TextureDesc texture_desc{};
    texture_desc.format = RendererInterface::RGBA16_FLOAT;
    texture_desc.width = 1u;
    texture_desc.height = 1u;
    texture_desc.data.assign(8u, 0);
    return m_resource_operator.CreateTexture(texture_desc);
}

unsigned long long RendererModuleLightmap::MakeInstancePrimitiveKey(unsigned stable_node_key, unsigned primitive_hash)
{
    return (static_cast<unsigned long long>(stable_node_key) << 32ull) | static_cast<unsigned long long>(primitive_hash);
}
