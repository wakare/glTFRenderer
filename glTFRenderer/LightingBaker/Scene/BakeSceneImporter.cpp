#include "BakeSceneImporter.h"

#include <algorithm>
#include <fstream>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <vector>
#include <utility>

#include "SceneFileLoader/glTFLoader.h"
#include "nlohmann_json/single_include/nlohmann/json.hpp"

namespace LightingBaker
{
    namespace
    {
        constexpr unsigned kInvalidUnsigned = 0xffffffffu;
        constexpr float kDefaultSpotOuterConeAngle = 0.78539816339f;
        using glTFAccessor = glTF_Element_Template<glTF_Element_Type::EAccessor>;

        struct AccessorDataView
        {
            const glTF_Element_Accessor_Base* accessor{nullptr};
            const std::uint8_t* data{nullptr};
            std::size_t stride{0};
            std::size_t count{0};
        };

        struct ParsedPunctualLightDefinition
        {
            bool valid{false};
            unsigned source_light_index{0xffffffffu};
            BakeSceneLightType type{BakeSceneLightType::Point};
            std::string name{};
            glm::fvec3 color{1.0f, 1.0f, 1.0f};
            float intensity{1.0f};
            float range{-1.0f};
            float inner_cone_angle{0.0f};
            float outer_cone_angle{kDefaultSpotOuterConeAngle};
        };

        struct ParsedPunctualLightRegistry
        {
            std::vector<ParsedPunctualLightDefinition> lights{};
            std::vector<int> node_light_indices{};
        };

        void AddMessage(std::vector<BakeSceneValidationMessage>& out_messages,
                        std::string code,
                        std::string message)
        {
            out_messages.push_back(BakeSceneValidationMessage{
                .code = std::move(code),
                .message = std::move(message),
            });
        }

        void AddError(BakeSceneImportResult& out_result, std::string code, std::string message)
        {
            AddMessage(out_result.errors, std::move(code), std::move(message));
        }

        void AddWarning(BakeSceneImportResult& out_result, std::string code, std::string message)
        {
            AddMessage(out_result.warnings, std::move(code), std::move(message));
        }

        void AddError(BakePrimitiveImportInfo& out_primitive, std::string code, std::string message)
        {
            AddMessage(out_primitive.errors, std::move(code), std::move(message));
        }

        void AddWarning(BakePrimitiveImportInfo& out_primitive, std::string code, std::string message)
        {
            AddMessage(out_primitive.warnings, std::move(code), std::move(message));
        }

        std::string ResolveNodeName(const glTF_Element_Node& node, unsigned node_index)
        {
            return node.name.empty() ? ("node_" + std::to_string(node_index)) : node.name;
        }

        std::string ResolveMeshName(const glTF_Element_Mesh& mesh, unsigned mesh_index)
        {
            return mesh.name.empty() ? ("mesh_" + std::to_string(mesh_index)) : mesh.name;
        }

        bool ResolveAccessorDataView(const glTFLoader& loader,
                                     const glTFHandle& accessor_handle,
                                     AccessorDataView& out_view,
                                     std::string& out_error)
        {
            if (!accessor_handle.IsValid())
            {
                out_error = "Accessor handle is invalid.";
                return false;
            }

            const unsigned accessor_index = static_cast<unsigned>(loader.ResolveIndex(accessor_handle));
            const auto& accessors = loader.GetAccessors();
            const auto& buffer_views = loader.GetBufferViews();
            const auto& buffer_data = loader.GetBufferData();
            if (accessor_index >= accessors.size())
            {
                out_error = "Accessor index is out of range.";
                return false;
            }

            const glTF_Element_Accessor_Base& accessor = *accessors[accessor_index];
            if (!accessor.buffer_view.IsValid())
            {
                out_error = "Accessor does not reference a buffer view.";
                return false;
            }

            const unsigned buffer_view_index = static_cast<unsigned>(loader.ResolveIndex(accessor.buffer_view));
            if (buffer_view_index >= buffer_views.size())
            {
                out_error = "Buffer view index is out of range.";
                return false;
            }

            const glTF_Element_BufferView& buffer_view = *buffer_views[buffer_view_index];
            glTFHandle resolved_buffer_handle = buffer_view.buffer;
            resolved_buffer_handle.node_index = loader.ResolveIndex(buffer_view.buffer);
            const auto buffer_it = buffer_data.find(resolved_buffer_handle);
            if (buffer_it == buffer_data.end())
            {
                out_error = "Buffer data for accessor was not found.";
                return false;
            }

            out_view.accessor = &accessor;
            out_view.count = accessor.count;
            out_view.stride = buffer_view.byte_stride != 0 ? buffer_view.byte_stride : accessor.GetElementByteSize();
            out_view.data = reinterpret_cast<const std::uint8_t*>(buffer_it->second.get()) +
                            buffer_view.byte_offset + accessor.byte_offset;
            return true;
        }

        bool ReadFloat2(const AccessorDataView& view, std::size_t index, glm::fvec2& out_value)
        {
            if (!view.accessor || index >= view.count)
            {
                return false;
            }

            if (view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec2)
            {
                return false;
            }

            float components[2]{};
            std::memcpy(components, view.data + index * view.stride, sizeof(components));
            out_value = glm::fvec2(components[0], components[1]);
            return true;
        }

        bool ReadFloat3(const AccessorDataView& view, std::size_t index, glm::fvec3& out_value)
        {
            if (!view.accessor || index >= view.count)
            {
                return false;
            }

            if (view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec3)
            {
                return false;
            }

            float components[3]{};
            std::memcpy(components, view.data + index * view.stride, sizeof(components));
            out_value = glm::fvec3(components[0], components[1], components[2]);
            return true;
        }

        bool ReadFloat4(const AccessorDataView& view, std::size_t index, glm::fvec4& out_value)
        {
            if (!view.accessor || index >= view.count)
            {
                return false;
            }

            if (view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec4 ||
                view.stride < sizeof(float) * 4u)
            {
                return false;
            }

            float components[4]{};
            std::memcpy(components, view.data + index * view.stride, sizeof(components));
            out_value = glm::fvec4(components[0], components[1], components[2], components[3]);
            return true;
        }

        bool ReadUnsignedIndex(const AccessorDataView& view, std::size_t index, unsigned& out_value)
        {
            if (!view.accessor || index >= view.count ||
                view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EScalar)
            {
                return false;
            }

            const std::uint8_t* address = view.data + index * view.stride;
            switch (view.accessor->component_type)
            {
            case glTFAccessor::glTF_Accessor_Component_Type::EUnsignedByte:
            {
                std::uint8_t value = 0;
                std::memcpy(&value, address, sizeof(value));
                out_value = value;
                return true;
            }
            case glTFAccessor::glTF_Accessor_Component_Type::EUnsignedShort:
            {
                std::uint16_t value = 0;
                std::memcpy(&value, address, sizeof(value));
                out_value = value;
                return true;
            }
            case glTFAccessor::glTF_Accessor_Component_Type::EUnsignedInt:
            {
                std::uint32_t value = 0;
                std::memcpy(&value, address, sizeof(value));
                out_value = value;
                return true;
            }
            default:
                return false;
            }
        }

        glm::fvec3 TransformPoint(const glm::fmat4x4& transform, const glm::fvec3& position)
        {
            const glm::fvec4 world_position = transform * glm::fvec4(position, 1.0f);
            return glm::fvec3(world_position.x, world_position.y, world_position.z);
        }

        glm::fvec3 TransformNormal(const glm::fmat4x4& transform, const glm::fvec3& normal)
        {
            const glm::fmat3 normal_matrix = glm::transpose(glm::inverse(glm::fmat3(transform)));
            const glm::fvec3 transformed = normal_matrix * normal;
            const float transformed_length = glm::length(transformed);
            if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) ||
                transformed_length <= 1e-8f)
            {
                return glm::fvec3(0.0f, 0.0f, 0.0f);
            }

            return transformed / transformed_length;
        }

        glm::fvec3 TransformDirection(const glm::fmat4x4& transform, const glm::fvec3& direction)
        {
            const glm::fvec3 transformed = glm::fmat3(transform) * direction;
            const float transformed_length = glm::length(transformed);
            if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) ||
                transformed_length <= 1e-8f)
            {
                return glm::fvec3(0.0f, 0.0f, -1.0f);
            }

            return transformed / transformed_length;
        }

        glm::fvec4 TransformTangent(const glm::fmat4x4& transform, const glm::fvec4& tangent)
        {
            const glm::fvec3 transformed_xyz = TransformDirection(transform, glm::fvec3(tangent.x, tangent.y, tangent.z));
            const float handedness = tangent.w >= 0.0f ? 1.0f : -1.0f;
            return glm::fvec4(transformed_xyz, handedness);
        }

        bool StartsWith(const std::string& value, const char* prefix)
        {
            const std::size_t prefix_length = std::char_traits<char>::length(prefix);
            return value.size() >= prefix_length && value.compare(0u, prefix_length, prefix) == 0;
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(),
                           value.end(),
                           value.begin(),
                           [](unsigned char character)
                           {
                               return static_cast<char>(std::tolower(character));
                           });
            return value;
        }

        std::string ResolveEmbeddedImageExtension(const glTF_Element_Image& image)
        {
            if (!image.uri.empty())
            {
                const std::filesystem::path image_path(image.uri);
                if (image_path.has_extension())
                {
                    return image_path.extension().string();
                }
            }

            const std::string mime_type = ToLowerAscii(image.mime_type);
            if (mime_type == "image/png")
            {
                return ".png";
            }
            if (mime_type == "image/jpeg" || mime_type == "image/jpg")
            {
                return ".jpg";
            }
            if (mime_type == "image/webp")
            {
                return ".webp";
            }
            if (mime_type == "image/bmp")
            {
                return ".bmp";
            }
            if (mime_type == "image/gif")
            {
                return ".gif";
            }
            if (mime_type == "image/ktx2")
            {
                return ".ktx2";
            }

            return {};
        }

        bool EnsureDirectoryExists(const std::filesystem::path& directory_path, std::string& out_error)
        {
            out_error.clear();
            if (directory_path.empty())
            {
                out_error = "Material texture cache directory is empty.";
                return false;
            }

            std::error_code error_code{};
            std::filesystem::create_directories(directory_path, error_code);
            if (error_code)
            {
                out_error = "Failed to create material texture cache directory: " + directory_path.string();
                return false;
            }

            return true;
        }

        bool WriteBinaryFile(const std::filesystem::path& file_path,
                             const unsigned char* data,
                             std::size_t size,
                             std::string& out_error)
        {
            if (data == nullptr || size == 0u)
            {
                out_error = "Embedded image buffer is empty.";
                return false;
            }

            if (!EnsureDirectoryExists(file_path.parent_path(), out_error))
            {
                return false;
            }

            std::ofstream stream(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!stream.is_open())
            {
                out_error = "Failed to open embedded image cache file for write: " + file_path.string();
                return false;
            }

            stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
            if (!stream.good())
            {
                out_error = "Failed to write embedded image cache file: " + file_path.string();
                return false;
            }

            return true;
        }

        bool ResolveEmbeddedImageUri(const glTFLoader& loader,
                                     const glTF_Element_Image& image,
                                     unsigned image_index,
                                     const std::filesystem::path& material_texture_cache_root,
                                     std::string& out_texture_uri,
                                     std::string& out_error)
        {
            out_texture_uri.clear();
            out_error.clear();

            if (material_texture_cache_root.empty())
            {
                out_error = "Material texture cache root is not configured.";
                return false;
            }

            if (!image.buffer_view.IsValid())
            {
                out_error = "Buffer-view-backed image is missing a valid bufferView.";
                return false;
            }

            const unsigned char* image_data = nullptr;
            std::size_t image_size = 0u;
            if (!loader.GetBufferViewData(image.buffer_view, image_data, image_size) ||
                image_data == nullptr ||
                image_size == 0u)
            {
                out_error = "Failed to read encoded image bytes from the referenced bufferView.";
                return false;
            }

            const std::string extension = ResolveEmbeddedImageExtension(image);
            if (extension.empty())
            {
                out_error = "Buffer-view-backed image is missing a supported mimeType.";
                return false;
            }

            const std::filesystem::path texture_path =
                (material_texture_cache_root / std::filesystem::path("embedded_image_" + std::to_string(image_index) + extension))
                    .lexically_normal();
            if (!WriteBinaryFile(texture_path, image_data, image_size, out_error))
            {
                return false;
            }

            out_texture_uri = texture_path.string();
            return true;
        }

        void ParsePunctualLightRegistry(const glTFLoader& loader,
                                        std::size_t node_count,
                                        ParsedPunctualLightRegistry& out_registry,
                                        BakeSceneImportResult& out_result)
        {
            out_registry = ParsedPunctualLightRegistry{};
            out_registry.node_light_indices.assign(node_count, -1);

            const std::string& source_json_text = loader.GetSourceJsonText();
            if (source_json_text.empty())
            {
                AddWarning(out_result,
                           "punctual_lights_source_unavailable",
                           "The loader did not preserve source JSON text. Direct-light import was skipped.");
                return;
            }

            nlohmann::json root{};
            try
            {
                root = nlohmann::json::parse(source_json_text);
            }
            catch (const std::exception& exception)
            {
                AddWarning(out_result,
                           "punctual_lights_parse_failed",
                           std::string("Failed to parse KHR_lights_punctual data. Direct-light import was skipped: ") +
                               exception.what());
                return;
            }

            if (!root.contains("extensions") || !root["extensions"].is_object())
            {
                return;
            }

            const auto extension_it = root["extensions"].find("KHR_lights_punctual");
            if (extension_it == root["extensions"].end() || !extension_it->is_object())
            {
                return;
            }

            const nlohmann::json& extension_root = *extension_it;
            const auto lights_it = extension_root.find("lights");
            if (lights_it == extension_root.end() || !lights_it->is_array())
            {
                return;
            }

            out_registry.lights.resize(lights_it->size());
            for (std::size_t light_index = 0; light_index < lights_it->size(); ++light_index)
            {
                const nlohmann::json& light_json = (*lights_it)[light_index];
                if (!light_json.is_object())
                {
                    AddWarning(out_result,
                               "punctual_light_invalid_object",
                               "Encountered a non-object entry in KHR_lights_punctual.lights; it was skipped.");
                    continue;
                }

                ParsedPunctualLightDefinition definition{};
                definition.source_light_index = static_cast<unsigned>(light_index);
                definition.name = light_json.value("name", "light_" + std::to_string(light_index));

                const std::string type_string = light_json.value("type", std::string{});
                if (type_string == "directional")
                {
                    definition.type = BakeSceneLightType::Directional;
                }
                else if (type_string == "point")
                {
                    definition.type = BakeSceneLightType::Point;
                }
                else if (type_string == "spot")
                {
                    definition.type = BakeSceneLightType::Spot;
                }
                else
                {
                    AddWarning(out_result,
                               "punctual_light_unsupported_type",
                               "Encountered an unsupported KHR_lights_punctual light type; it was skipped.");
                    continue;
                }

                if (light_json.contains("color") && light_json["color"].is_array() && light_json["color"].size() >= 3u)
                {
                    const nlohmann::json& color = light_json["color"];
                    if (color[0].is_number() && color[1].is_number() && color[2].is_number())
                    {
                        definition.color = glm::fvec3(
                            color[0].get<float>(),
                            color[1].get<float>(),
                            color[2].get<float>());
                    }
                }

                if (light_json.contains("intensity") && light_json["intensity"].is_number())
                {
                    definition.intensity = light_json["intensity"].get<float>();
                }

                if (light_json.contains("range") && light_json["range"].is_number())
                {
                    definition.range = light_json["range"].get<float>();
                }

                if (definition.type == BakeSceneLightType::Spot &&
                    light_json.contains("spot") &&
                    light_json["spot"].is_object())
                {
                    const nlohmann::json& spot_json = light_json["spot"];
                    if (spot_json.contains("innerConeAngle") && spot_json["innerConeAngle"].is_number())
                    {
                        definition.inner_cone_angle = spot_json["innerConeAngle"].get<float>();
                    }
                    if (spot_json.contains("outerConeAngle") && spot_json["outerConeAngle"].is_number())
                    {
                        definition.outer_cone_angle = spot_json["outerConeAngle"].get<float>();
                    }
                }

                const bool color_finite =
                    std::isfinite(definition.color.x) &&
                    std::isfinite(definition.color.y) &&
                    std::isfinite(definition.color.z);
                const bool intensity_finite = std::isfinite(definition.intensity);
                const bool range_valid = !std::isfinite(definition.range) || definition.range > 0.0f || definition.range < 0.0f;
                if (!color_finite || !intensity_finite || !range_valid)
                {
                    AddWarning(out_result,
                               "punctual_light_invalid_parameters",
                               "Encountered a KHR_lights_punctual light with non-finite parameters; it was skipped.");
                    continue;
                }

                definition.color = glm::max(definition.color, glm::fvec3(0.0f, 0.0f, 0.0f));
                definition.intensity = (std::max)(0.0f, definition.intensity);
                definition.range = (definition.range > 0.0f && std::isfinite(definition.range)) ? definition.range : -1.0f;
                definition.inner_cone_angle = glm::clamp(definition.inner_cone_angle, 0.0f, kDefaultSpotOuterConeAngle);
                definition.outer_cone_angle =
                    glm::clamp(definition.outer_cone_angle, definition.inner_cone_angle, 1.57079632679f);
                definition.valid = true;
                out_registry.lights[light_index] = definition;
            }

            if (!root.contains("nodes") || !root["nodes"].is_array())
            {
                return;
            }

            const nlohmann::json& nodes_json = root["nodes"];
            const std::size_t parsed_node_count = (std::min)(node_count, nodes_json.size());
            for (std::size_t node_index = 0; node_index < parsed_node_count; ++node_index)
            {
                const nlohmann::json& node_json = nodes_json[node_index];
                if (!node_json.is_object() || !node_json.contains("extensions") || !node_json["extensions"].is_object())
                {
                    continue;
                }

                const auto node_light_extension_it = node_json["extensions"].find("KHR_lights_punctual");
                if (node_light_extension_it == node_json["extensions"].end() || !node_light_extension_it->is_object())
                {
                    continue;
                }

                const auto light_reference_it = node_light_extension_it->find("light");
                if (light_reference_it == node_light_extension_it->end() || !light_reference_it->is_number_unsigned())
                {
                    continue;
                }

                out_registry.node_light_indices[node_index] = static_cast<int>(light_reference_it->get<unsigned>());
            }
        }

        void CollectNodePunctualLightInstance(const ParsedPunctualLightRegistry& light_registry,
                                              const glTF_Element_Node& node,
                                              unsigned node_index,
                                              const glm::fmat4x4& world_transform,
                                              BakeSceneImportResult& out_result)
        {
            if (node_index >= light_registry.node_light_indices.size())
            {
                return;
            }

            const int referenced_light_index = light_registry.node_light_indices[node_index];
            if (referenced_light_index < 0)
            {
                return;
            }

            const std::size_t light_index = static_cast<std::size_t>(referenced_light_index);
            if (light_index >= light_registry.lights.size())
            {
                AddWarning(out_result,
                           "punctual_light_reference_out_of_range",
                           "Node referenced a KHR_lights_punctual light index outside the parsed light array.");
                return;
            }

            const ParsedPunctualLightDefinition& definition = light_registry.lights[light_index];
            if (!definition.valid)
            {
                return;
            }

            BakeSceneLightImportInfo light_instance{};
            light_instance.light_index = definition.source_light_index;
            light_instance.stable_node_key = node_index;
            light_instance.type = definition.type;
            light_instance.light_name = definition.name;
            light_instance.node_name = ResolveNodeName(node, node_index);
            light_instance.color = definition.color;
            light_instance.intensity = definition.intensity;
            light_instance.range = definition.range;
            light_instance.world_position = TransformPoint(world_transform, glm::fvec3(0.0f, 0.0f, 0.0f));
            light_instance.world_direction = TransformDirection(world_transform, glm::fvec3(0.0f, 0.0f, -1.0f));
            light_instance.spot_inner_cone_angle = definition.inner_cone_angle;
            light_instance.spot_outer_cone_angle = definition.outer_cone_angle;
            out_result.punctual_lights.push_back(light_instance);
            ++out_result.punctual_light_count;

            switch (light_instance.type)
            {
            case BakeSceneLightType::Directional:
                ++out_result.directional_light_count;
                break;
            case BakeSceneLightType::Point:
                ++out_result.point_light_count;
                break;
            case BakeSceneLightType::Spot:
                ++out_result.spot_light_count;
                break;
            }
        }

        bool ResolveTextureUri(const glTFLoader& loader,
                               const std::filesystem::path& material_texture_cache_root,
                               const glTF_TextureInfo_Base& texture_info,
                               std::string& out_texture_uri,
                               std::string& out_error)
        {
            out_texture_uri.clear();
            out_error.clear();
            if (!texture_info.index.IsValid())
            {
                out_error = "Texture handle is invalid.";
                return false;
            }

            const auto& textures = loader.GetTextures();
            const unsigned texture_index = static_cast<unsigned>(loader.ResolveIndex(texture_info.index));
            if (texture_index >= textures.size())
            {
                out_error = "Texture index is out of range.";
                return false;
            }

            const auto& texture = *textures[texture_index];
            if (!texture.source.IsValid())
            {
                out_error = "Texture source handle is invalid.";
                return false;
            }

            const auto& images = loader.GetImages();
            const unsigned image_index = static_cast<unsigned>(loader.ResolveIndex(texture.source));
            if (image_index >= images.size())
            {
                out_error = "Texture image index is out of range.";
                return false;
            }

            const auto& image = *images[image_index];
            if (!image.uri.empty())
            {
                if (StartsWith(image.uri, "data:"))
                {
                    out_error = "Data URI-backed images are not supported yet.";
                    return false;
                }

                const std::filesystem::path texture_path =
                    (std::filesystem::path(loader.GetSceneFileDirectory()) / std::filesystem::path(image.uri)).lexically_normal();
                out_texture_uri = texture_path.string();
                return true;
            }

            if (image.buffer_view.IsValid())
            {
                return ResolveEmbeddedImageUri(
                    loader,
                    image,
                    image_index,
                    material_texture_cache_root,
                    out_texture_uri,
                    out_error);
            }

            out_error = "Image does not provide a URI or buffer view.";
            return false;
        }

        bool ResolveSupportedMaterialTexture(const glTFLoader& loader,
                                             const std::filesystem::path& material_texture_cache_root,
                                             BakePrimitiveImportInfo& primitive,
                                             const glTF_TextureInfo_Base& texture_info,
                                             const char* texture_label,
                                             unsigned& out_texcoord_index,
                                             bool& out_has_texture,
                                             std::string& out_texture_uri)
        {
            out_texcoord_index = 0u;
            out_has_texture = false;
            out_texture_uri.clear();
            if (!texture_info.index.IsValid())
            {
                return true;
            }

            out_texcoord_index = texture_info.texCoord_index;
            if (texture_info.texCoord_index > 1u)
            {
                AddWarning(primitive,
                           std::string(texture_label) + "_texcoord",
                           std::string(texture_label) + " uses TEXCOORD index > 1, which is not supported yet. Falling back to factor-only shading.");
                return true;
            }

            std::string texture_uri{};
            std::string texture_error{};
            if (!ResolveTextureUri(loader, material_texture_cache_root, texture_info, texture_uri, texture_error))
            {
                AddWarning(primitive,
                           std::string(texture_label) + "_source",
                           "Failed to resolve " + std::string(texture_label) + " for the baker: " + texture_error);
                return true;
            }

            if (!std::filesystem::exists(texture_uri))
            {
                AddWarning(primitive,
                           std::string(texture_label) + "_missing",
                           "Resolved " + std::string(texture_label) + " file does not exist on disk. Falling back to factor-only shading.");
                return true;
            }

            out_has_texture = true;
            out_texture_uri = std::move(texture_uri);
            return true;
        }

        void PopulatePrimitiveMaterial(const glTFLoader& loader,
                                       const std::filesystem::path& material_texture_cache_root,
                                       BakePrimitiveImportInfo& primitive,
                                       BakeMaterialImportInfo& out_material)
        {
            out_material = BakeMaterialImportInfo{};
            out_material.material_index = primitive.material_index;

            if (primitive.material_index == kInvalidUnsigned)
            {
                return;
            }

            const auto& materials = loader.GetMaterials();
            if (primitive.material_index >= materials.size() || !materials[primitive.material_index])
            {
                return;
            }

            const glTF_Element_Material& material = *materials[primitive.material_index];
            out_material.base_color_factor = material.pbr.base_color_factor;
            out_material.emissive_factor = material.emissive_factor;
            out_material.metallic_factor = material.pbr.metallic_factor;
            out_material.roughness_factor = material.pbr.roughness_factor;
            out_material.normal_texture_scale = material.normal_texture.scale;
            out_material.alpha_cutoff = glm::clamp(material.alpha_cutoff, 0.0f, 1.0f);
            out_material.double_sided = material.double_sided;
            out_material.alpha_masked = material.alpha_mode == "MASK";
            out_material.alpha_blended = material.alpha_mode == "BLEND";
            ResolveSupportedMaterialTexture(loader,
                                            material_texture_cache_root,
                                            primitive,
                                            material.pbr.base_color_texture,
                                            "baseColorTexture",
                                            out_material.base_color_texture_texcoord,
                                            out_material.has_base_color_texture,
                                            out_material.base_color_texture_uri);
            ResolveSupportedMaterialTexture(loader,
                                            material_texture_cache_root,
                                            primitive,
                                            material.pbr.metallic_roughness_texture,
                                            "metallicRoughnessTexture",
                                            out_material.metallic_roughness_texture_texcoord,
                                            out_material.has_metallic_roughness_texture,
                                            out_material.metallic_roughness_texture_uri);
            ResolveSupportedMaterialTexture(loader,
                                            material_texture_cache_root,
                                            primitive,
                                            material.normal_texture,
                                            "normalTexture",
                                            out_material.normal_texture_texcoord,
                                            out_material.has_normal_texture,
                                            out_material.normal_texture_uri);
            ResolveSupportedMaterialTexture(loader,
                                            material_texture_cache_root,
                                            primitive,
                                            material.emissive_texture,
                                            "emissiveTexture",
                                            out_material.emissive_texture_texcoord,
                                            out_material.has_emissive_texture,
                                            out_material.emissive_texture_uri);
        }

        void ValidatePrimitiveGeometry(const glTFLoader& loader,
                                       const glTF_Primitive& primitive,
                                       const glm::fmat4x4& world_transform,
                                       BakePrimitiveImportInfo& out_primitive)
        {
            const auto it_position = primitive.attributes.find(glTF_Attribute_POSITION::attribute_type_id);
            const auto it_normal = primitive.attributes.find(glTF_Attribute_NORMAL::attribute_type_id);
            const auto it_tangent = primitive.attributes.find(glTF_Attribute_TANGENT::attribute_type_id);
            const auto it_uv0 = primitive.attributes.find(glTF_Attribute_TEXCOORD_0::attribute_type_id);
            const auto it_uv1 = primitive.attributes.find(glTF_Attribute_TEXCOORD_1::attribute_type_id);
            if (it_position == primitive.attributes.end() || it_uv1 == primitive.attributes.end() || !primitive.indices.IsValid())
            {
                return;
            }

            AccessorDataView position_view{};
            AccessorDataView normal_view{};
            AccessorDataView tangent_view{};
            AccessorDataView uv0_view{};
            AccessorDataView uv1_view{};
            AccessorDataView index_view{};
            std::string accessor_error{};
            if (!ResolveAccessorDataView(loader, it_position->second, position_view, accessor_error))
            {
                AddError(out_primitive, "position_accessor", accessor_error);
                return;
            }
            if (!ResolveAccessorDataView(loader, it_uv1->second, uv1_view, accessor_error))
            {
                AddError(out_primitive, "uv1_accessor", accessor_error);
                return;
            }
            if (!ResolveAccessorDataView(loader, primitive.indices, index_view, accessor_error))
            {
                AddError(out_primitive, "index_accessor", accessor_error);
                return;
            }

            bool use_normal_accessor = false;
            bool use_tangent_accessor = false;
            bool use_uv0_accessor = false;
            if (it_normal != primitive.attributes.end())
            {
                out_primitive.has_normals = true;
                if (!ResolveAccessorDataView(loader, it_normal->second, normal_view, accessor_error))
                {
                    AddWarning(out_primitive,
                               "normal_accessor",
                               "Failed to resolve NORMAL accessor. Falling back to generated shading normals.");
                }
                else if (normal_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                         normal_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec3)
                {
                    AddWarning(out_primitive,
                               "normal_format",
                               "NORMAL accessor must be float3. Falling back to generated shading normals.");
                }
                else if (normal_view.count != position_view.count)
                {
                    AddWarning(out_primitive,
                               "normal_count_mismatch",
                               "NORMAL count does not match POSITION count. Falling back to generated shading normals.");
                }
                else
                {
                    use_normal_accessor = true;
                }
            }

            if (it_tangent != primitive.attributes.end())
            {
                if (!ResolveAccessorDataView(loader, it_tangent->second, tangent_view, accessor_error))
                {
                    AddWarning(out_primitive,
                               "tangent_accessor",
                               "Failed to resolve TANGENT accessor. Ignoring normalTexture shading.");
                }
                else if (tangent_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                         tangent_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec4)
                {
                    AddWarning(out_primitive,
                               "tangent_format",
                               "TANGENT accessor must be float4. Ignoring normalTexture shading.");
                }
                else if (tangent_view.count != position_view.count)
                {
                    AddWarning(out_primitive,
                               "tangent_count_mismatch",
                               "TANGENT count does not match POSITION count. Ignoring normalTexture shading.");
                }
                else
                {
                    out_primitive.has_tangents = true;
                    use_tangent_accessor = true;
                }
            }

            if (it_uv0 != primitive.attributes.end())
            {
                if (!ResolveAccessorDataView(loader, it_uv0->second, uv0_view, accessor_error))
                {
                    AddWarning(out_primitive,
                               "uv0_accessor",
                               "Failed to resolve TEXCOORD_0 accessor. Ignoring TEXCOORD_0 for baker shading textures.");
                }
                else if (uv0_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                         uv0_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec2)
                {
                    AddWarning(out_primitive,
                               "uv0_format",
                               "TEXCOORD_0 accessor must be float2 for baker shading textures. Ignoring TEXCOORD_0.");
                }
                else if (uv0_view.count != position_view.count)
                {
                    AddWarning(out_primitive,
                               "uv0_count_mismatch",
                               "TEXCOORD_0 count does not match POSITION count. Ignoring TEXCOORD_0 for baker shading textures.");
                }
                else
                {
                    use_uv0_accessor = true;
                }
            }

            if (position_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                position_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec3)
            {
                AddError(out_primitive, "position_format", "POSITION accessor must be float3.");
                return;
            }

            if (uv1_view.accessor->component_type != glTFAccessor::glTF_Accessor_Component_Type::EFloat ||
                uv1_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EVec2)
            {
                AddError(out_primitive, "uv1_format", "TEXCOORD_1 accessor must be float2.");
                return;
            }

            if (position_view.count == 0)
            {
                AddError(out_primitive, "vertex_count", "Primitive has zero vertices.");
                return;
            }

            if (uv1_view.count != position_view.count)
            {
                AddError(out_primitive, "uv1_count_mismatch", "TEXCOORD_1 count does not match POSITION count.");
                return;
            }

            if (index_view.accessor->element_type != glTFAccessor::glTF_Accessor_Element_Type::EScalar)
            {
                AddError(out_primitive, "index_format", "Index accessor must be scalar.");
                return;
            }

            if (index_view.count % 3 != 0)
            {
                AddError(out_primitive, "index_topology", "Indexed triangle list requires index count divisible by 3.");
                return;
            }

            out_primitive.vertex_count = static_cast<unsigned>(position_view.count);
            out_primitive.index_count = static_cast<unsigned>(index_view.count);
            out_primitive.geometry.world_positions.resize(position_view.count);
            out_primitive.geometry.world_normals.resize(position_view.count, glm::fvec3(0.0f, 0.0f, 0.0f));
            if (use_tangent_accessor)
            {
                out_primitive.geometry.world_tangents.resize(position_view.count, glm::fvec4(1.0f, 0.0f, 0.0f, 1.0f));
            }
            if (use_uv0_accessor)
            {
                out_primitive.geometry.uv0_vertices.resize(position_view.count);
            }
            out_primitive.geometry.uv1_vertices.resize(uv1_view.count);
            out_primitive.geometry.triangle_indices.resize(index_view.count);
            std::vector<std::uint8_t> normal_valid_mask(position_view.count, 0u);
            std::vector<glm::fvec3> generated_normal_accum(position_view.count, glm::fvec3(0.0f, 0.0f, 0.0f));

            bool uv_bounds_initialized = false;
            glm::fvec2 uv_min{0.0f, 0.0f};
            glm::fvec2 uv_max{0.0f, 0.0f};
            for (std::size_t vertex_index = 0; vertex_index < uv1_view.count; ++vertex_index)
            {
                glm::fvec2 uv{};
                glm::fvec3 position{};
                if (!ReadFloat2(uv1_view, vertex_index, uv))
                {
                    AddError(out_primitive, "uv1_read", "Failed to read TEXCOORD_1 vertex data.");
                    return;
                }

                if (!ReadFloat3(position_view, vertex_index, position))
                {
                    AddError(out_primitive, "position_read", "Failed to read POSITION vertex data.");
                    return;
                }

                out_primitive.geometry.world_positions[vertex_index] = TransformPoint(world_transform, position);
                out_primitive.geometry.uv1_vertices[vertex_index] = uv;
                if (use_uv0_accessor)
                {
                    glm::fvec2 uv0{};
                    if (!ReadFloat2(uv0_view, vertex_index, uv0))
                    {
                        AddWarning(out_primitive,
                                   "uv0_read",
                                   "Failed to read TEXCOORD_0 vertex data. Ignoring TEXCOORD_0 for baker shading textures.");
                        use_uv0_accessor = false;
                        out_primitive.geometry.uv0_vertices.clear();
                    }
                    else
                    {
                        out_primitive.geometry.uv0_vertices[vertex_index] = uv0;
                    }
                }
                if (use_normal_accessor)
                {
                    glm::fvec3 imported_normal{};
                    if (ReadFloat3(normal_view, vertex_index, imported_normal))
                    {
                        const glm::fvec3 world_normal = TransformNormal(world_transform, imported_normal);
                        if (glm::dot(world_normal, world_normal) > 1e-8f)
                        {
                            out_primitive.geometry.world_normals[vertex_index] = world_normal;
                            normal_valid_mask[vertex_index] = 1u;
                        }
                    }
                }
                if (use_tangent_accessor)
                {
                    glm::fvec4 imported_tangent{};
                    if (!ReadFloat4(tangent_view, vertex_index, imported_tangent))
                    {
                        AddWarning(out_primitive,
                                   "tangent_read",
                                   "Failed to read TANGENT vertex data. Ignoring normalTexture shading.");
                        use_tangent_accessor = false;
                        out_primitive.has_tangents = false;
                        out_primitive.geometry.world_tangents.clear();
                    }
                    else
                    {
                        out_primitive.geometry.world_tangents[vertex_index] = TransformTangent(world_transform, imported_tangent);
                    }
                }

                if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
                {
                    ++out_primitive.uv1_non_finite_vertex_count;
                    continue;
                }

                if (!uv_bounds_initialized)
                {
                    uv_min = uv;
                    uv_max = uv;
                    uv_bounds_initialized = true;
                }
                else
                {
                    uv_min = glm::min(uv_min, uv);
                    uv_max = glm::max(uv_max, uv);
                }

                if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
                {
                    ++out_primitive.uv1_out_of_range_vertex_count;
                }
            }

            out_primitive.uv1_min = uv_bounds_initialized ? uv_min : glm::fvec2(0.0f, 0.0f);
            out_primitive.uv1_max = uv_bounds_initialized ? uv_max : glm::fvec2(0.0f, 0.0f);

            const float uv_area_epsilon = 1e-10f;
            const float position_area_epsilon = 1e-12f;
            for (std::size_t triangle_index = 0; triangle_index < index_view.count; triangle_index += 3)
            {
                unsigned triangle_indices[3]{};
                if (!ReadUnsignedIndex(index_view, triangle_index, triangle_indices[0]) ||
                    !ReadUnsignedIndex(index_view, triangle_index + 1, triangle_indices[1]) ||
                    !ReadUnsignedIndex(index_view, triangle_index + 2, triangle_indices[2]))
                {
                    AddError(out_primitive, "index_read", "Failed to read primitive index data.");
                    return;
                }

                out_primitive.geometry.triangle_indices[triangle_index] = triangle_indices[0];
                out_primitive.geometry.triangle_indices[triangle_index + 1] = triangle_indices[1];
                out_primitive.geometry.triangle_indices[triangle_index + 2] = triangle_indices[2];

                if (triangle_indices[0] >= position_view.count ||
                    triangle_indices[1] >= position_view.count ||
                    triangle_indices[2] >= position_view.count)
                {
                    AddError(out_primitive, "index_range", "Primitive index references a vertex outside accessor range.");
                    return;
                }

                const glm::fvec2 triangle_uv[3] = {
                    out_primitive.geometry.uv1_vertices[triangle_indices[0]],
                    out_primitive.geometry.uv1_vertices[triangle_indices[1]],
                    out_primitive.geometry.uv1_vertices[triangle_indices[2]],
                };
                const glm::fvec3 triangle_positions[3] = {
                    out_primitive.geometry.world_positions[triangle_indices[0]],
                    out_primitive.geometry.world_positions[triangle_indices[1]],
                    out_primitive.geometry.world_positions[triangle_indices[2]],
                };

                const glm::fvec2 uv_edge_01 = triangle_uv[1] - triangle_uv[0];
                const glm::fvec2 uv_edge_02 = triangle_uv[2] - triangle_uv[0];
                const float uv_area = std::abs(uv_edge_01.x * uv_edge_02.y - uv_edge_01.y * uv_edge_02.x);
                if (uv_area <= uv_area_epsilon)
                {
                    ++out_primitive.degenerate_uv_triangle_count;
                }

                const glm::fvec3 position_edge_01 = triangle_positions[1] - triangle_positions[0];
                const glm::fvec3 position_edge_02 = triangle_positions[2] - triangle_positions[0];
                const glm::fvec3 normal_cross = glm::cross(position_edge_01, position_edge_02);
                const float position_area_sq = glm::dot(normal_cross, normal_cross);
                if (position_area_sq <= position_area_epsilon)
                {
                    ++out_primitive.degenerate_position_triangle_count;
                }
                else
                {
                    generated_normal_accum[triangle_indices[0]] += normal_cross;
                    generated_normal_accum[triangle_indices[1]] += normal_cross;
                    generated_normal_accum[triangle_indices[2]] += normal_cross;
                }
            }

            for (std::size_t vertex_index = 0; vertex_index < out_primitive.geometry.world_normals.size(); ++vertex_index)
            {
                if (normal_valid_mask[vertex_index] != 0u)
                {
                    continue;
                }

                const glm::fvec3 generated_normal = generated_normal_accum[vertex_index];
                const float generated_length = glm::length(generated_normal);
                out_primitive.geometry.world_normals[vertex_index] =
                    generated_length > 1e-8f
                        ? (generated_normal / generated_length)
                        : glm::fvec3(0.0f, 1.0f, 0.0f);
            }

            if (out_primitive.uv1_non_finite_vertex_count > 0)
            {
                AddError(out_primitive, "uv1_non_finite", "TEXCOORD_1 contains non-finite vertex values.");
            }

            if (out_primitive.uv1_out_of_range_vertex_count > 0)
            {
                AddError(out_primitive, "uv1_out_of_range", "TEXCOORD_1 contains vertices outside the [0, 1] range.");
            }

            if (out_primitive.degenerate_uv_triangle_count > 0)
            {
                AddError(out_primitive, "uv1_degenerate_triangles", "TEXCOORD_1 contains triangles with zero or near-zero UV area.");
            }

            if (out_primitive.degenerate_position_triangle_count > 0)
            {
                AddWarning(out_primitive, "position_degenerate_triangles", "Primitive contains triangles with zero or near-zero geometric area.");
            }
        }

        void ValidatePrimitive(const glTFLoader& loader,
                               const std::filesystem::path& material_texture_cache_root,
                               const glTF_Primitive& primitive,
                               const glm::fmat4x4& world_transform,
                               BakePrimitiveImportInfo& out_primitive)
        {
            out_primitive.has_texcoord0 =
                primitive.attributes.contains(glTF_Attribute_TEXCOORD_0::attribute_type_id);
            out_primitive.has_texcoord1 =
                primitive.attributes.contains(glTF_Attribute_TEXCOORD_1::attribute_type_id);

            if (primitive.mode != glTF_Primitive::ETriangles)
            {
                AddError(out_primitive, "primitive_mode", "Only triangle-list primitives are supported in the baker.");
            }

            if (!primitive.attributes.contains(glTF_Attribute_POSITION::attribute_type_id))
            {
                AddError(out_primitive, "missing_position", "Primitive is missing POSITION.");
            }

            if (!primitive.indices.IsValid())
            {
                AddError(out_primitive, "missing_indices", "Primitive is missing indices.");
            }

            if (!out_primitive.has_texcoord1)
            {
                AddError(out_primitive, "missing_uv1", "Primitive is missing TEXCOORD_1.");
            }

            PopulatePrimitiveMaterial(loader, material_texture_cache_root, out_primitive, out_primitive.material);
            ValidatePrimitiveGeometry(loader, primitive, world_transform, out_primitive);
            if (out_primitive.material.has_base_color_texture)
            {
                const bool has_required_texcoord =
                    out_primitive.material.base_color_texture_texcoord == 0u
                        ? out_primitive.geometry.uv0_vertices.size() == out_primitive.geometry.world_positions.size()
                        : out_primitive.geometry.uv1_vertices.size() == out_primitive.geometry.world_positions.size();
                if (!has_required_texcoord)
                {
                    AddWarning(out_primitive,
                               out_primitive.material.base_color_texture_texcoord == 0u
                                   ? "base_color_texture_missing_uv0"
                                   : "base_color_texture_missing_uv1",
                               out_primitive.material.base_color_texture_texcoord == 0u
                                   ? "baseColorTexture references TEXCOORD_0, but a valid TEXCOORD_0 stream was not imported. Falling back to factor-only shading."
                                   : "baseColorTexture references TEXCOORD_1, but a valid TEXCOORD_1 stream was not imported. Falling back to factor-only shading.");
                    out_primitive.material.has_base_color_texture = false;
                    out_primitive.material.base_color_texture_uri.clear();
                }
            }
            if (out_primitive.material.has_normal_texture)
            {
                const bool has_required_texcoord =
                    out_primitive.material.normal_texture_texcoord == 0u
                        ? out_primitive.geometry.uv0_vertices.size() == out_primitive.geometry.world_positions.size()
                        : out_primitive.geometry.uv1_vertices.size() == out_primitive.geometry.world_positions.size();
                if (!has_required_texcoord)
                {
                    AddWarning(out_primitive,
                               out_primitive.material.normal_texture_texcoord == 0u
                                   ? "normal_texture_missing_uv0"
                                   : "normal_texture_missing_uv1",
                               out_primitive.material.normal_texture_texcoord == 0u
                                   ? "normalTexture references TEXCOORD_0, but a valid TEXCOORD_0 stream was not imported. Falling back to vertex normals."
                                   : "normalTexture references TEXCOORD_1, but a valid TEXCOORD_1 stream was not imported. Falling back to vertex normals.");
                    out_primitive.material.has_normal_texture = false;
                    out_primitive.material.normal_texture_uri.clear();
                }
                else if (out_primitive.geometry.world_tangents.size() != out_primitive.geometry.world_positions.size())
                {
                    AddWarning(out_primitive,
                               "normal_texture_missing_tangent",
                               "normalTexture requires a valid TANGENT stream. Falling back to vertex normals.");
                    out_primitive.material.has_normal_texture = false;
                    out_primitive.material.normal_texture_uri.clear();
                }
            }
            if (out_primitive.material.has_emissive_texture)
            {
                const bool has_required_texcoord =
                    out_primitive.material.emissive_texture_texcoord == 0u
                        ? out_primitive.geometry.uv0_vertices.size() == out_primitive.geometry.world_positions.size()
                        : out_primitive.geometry.uv1_vertices.size() == out_primitive.geometry.world_positions.size();
                if (!has_required_texcoord)
                {
                    AddWarning(out_primitive,
                               out_primitive.material.emissive_texture_texcoord == 0u
                                   ? "emissive_texture_missing_uv0"
                                   : "emissive_texture_missing_uv1",
                               out_primitive.material.emissive_texture_texcoord == 0u
                                   ? "emissiveTexture references TEXCOORD_0, but a valid TEXCOORD_0 stream was not imported. Falling back to factor-only shading."
                                   : "emissiveTexture references TEXCOORD_1, but a valid TEXCOORD_1 stream was not imported. Falling back to factor-only shading.");
                    out_primitive.material.has_emissive_texture = false;
                    out_primitive.material.emissive_texture_uri.clear();
                }
            }
            out_primitive.can_emit_lightmap_binding = out_primitive.errors.empty();
        }

        void CollectNodePrimitiveInstances(const glTFLoader& loader,
                                           const ParsedPunctualLightRegistry& light_registry,
                                           const std::filesystem::path& material_texture_cache_root,
                                           const glTFHandle& node_handle,
                                           const glm::fmat4x4& parent_world_transform,
                                           BakeSceneImportResult& out_result)
        {
            const unsigned node_index = static_cast<unsigned>(loader.ResolveIndex(node_handle));
            const auto& nodes = loader.GetNodes();
            const auto& meshes = loader.GetMeshes();
            if (node_index >= nodes.size())
            {
                AddError(out_result, "node_index", "Scene node index is out of range during import traversal.");
                return;
            }

            const glTF_Element_Node& node = *nodes[node_index];
            const glm::fmat4x4 world_transform = parent_world_transform * node.transform.GetMatrix();
            CollectNodePunctualLightInstance(light_registry, node, node_index, world_transform, out_result);
            for (const glTFHandle& mesh_handle : node.meshes)
            {
                const unsigned mesh_index = static_cast<unsigned>(loader.ResolveIndex(mesh_handle));
                if (mesh_index >= meshes.size())
                {
                    AddError(out_result, "mesh_index", "Node references a mesh outside mesh array range.");
                    continue;
                }

                const glTF_Element_Mesh& mesh = *meshes[mesh_index];
                for (unsigned primitive_index = 0; primitive_index < mesh.primitives.size(); ++primitive_index)
                {
                    const glTF_Primitive& primitive = mesh.primitives[primitive_index];
                    BakePrimitiveImportInfo primitive_info{};
                    primitive_info.stable_node_key = node_index;
                    primitive_info.primitive_hash = primitive.Hash();
                    primitive_info.mesh_index = mesh_index;
                    primitive_info.primitive_index = primitive_index;
                    primitive_info.material_index = primitive.material.IsValid()
                        ? static_cast<unsigned>(loader.ResolveIndex(primitive.material))
                        : kInvalidUnsigned;
                    primitive_info.node_name = ResolveNodeName(node, node_index);
                    primitive_info.mesh_name = ResolveMeshName(mesh, mesh_index);

                    ValidatePrimitive(loader, material_texture_cache_root, primitive, world_transform, primitive_info);
                    if (primitive_info.can_emit_lightmap_binding)
                    {
                        ++out_result.valid_lightmap_primitive_count;
                    }

                    ++out_result.instance_primitive_count;
                    out_result.primitive_instances.push_back(std::move(primitive_info));
                }
            }

            for (const glTFHandle& child_handle : node.children)
            {
                CollectNodePrimitiveInstances(
                    loader,
                    light_registry,
                    material_texture_cache_root,
                    child_handle,
                    world_transform,
                    out_result);
            }
        }
    }

    bool BakeSceneImportResult::HasErrors() const
    {
        if (!errors.empty())
        {
            return true;
        }

        for (const BakePrimitiveImportInfo& primitive : primitive_instances)
        {
            if (!primitive.errors.empty())
            {
                return true;
            }
        }

        return false;
    }

    bool BakeSceneImportResult::HasValidationErrors() const
    {
        return HasErrors();
    }

    bool BakeSceneImporter::ImportScene(const BakeSceneImportRequest& request,
                                        BakeSceneImportResult& out_result,
                                        std::wstring& out_error) const
    {
        out_result = BakeSceneImportResult{};
        out_result.scene_path = request.scene_path;

        glTFLoader loader{};
        if (!loader.LoadFile(request.scene_path.string()))
        {
            AddError(out_result, "load_file", "Failed to load glTF scene file.");
            out_error = L"Failed to load scene file: " + request.scene_path.native();
            return false;
        }

        out_result.load_succeeded = true;
        out_result.node_count = static_cast<unsigned>(loader.GetNodes().size());
        out_result.mesh_count = static_cast<unsigned>(loader.GetMeshes().size());

        ParsedPunctualLightRegistry light_registry{};
        ParsePunctualLightRegistry(loader,
                                   loader.GetNodes().size(),
                                   light_registry,
                                   out_result);

        const glTF_Element_Scene& default_scene = loader.GetDefaultScene();
        if (default_scene.root_nodes.empty())
        {
            AddError(out_result, "empty_scene", "Default scene does not contain any root nodes.");
        }

        for (const glTFHandle& root_node_handle : default_scene.root_nodes)
        {
            CollectNodePrimitiveInstances(
                loader,
                light_registry,
                request.material_texture_cache_root,
                root_node_handle,
                glm::fmat4x4(1.0f),
                out_result);
        }

        if (out_result.instance_primitive_count == 0)
        {
            AddError(out_result, "empty_primitives", "Scene does not contain any mesh primitives.");
        }

        if (out_result.valid_lightmap_primitive_count == 0 && out_result.load_succeeded)
        {
            AddWarning(out_result,
                       "no_valid_lightmap_primitives",
                       "No primitive passed the current TEXCOORD_1 validation gate.");
        }

        return true;
    }
}
