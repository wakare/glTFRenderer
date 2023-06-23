#include "glTFLoader.h"

#include <fstream>
#include <iostream>

#include "../glTFUtils/glTFLog.h"
#include "nlohmann_json/single_include/nlohmann/json.hpp"

#define glTF_PROCESS_SCALAR(JSON_ELEMENT, SCALAR_NAME, SCALAR_TYPE, RESULT) \
    if ((JSON_ELEMENT).contains(SCALAR_NAME)) \
    { \
        (RESULT) = (JSON_ELEMENT)[SCALAR_NAME].get<SCALAR_TYPE>(); \
    }

// glTFHandle must be string or unsigned type
#define glTF_PROCESS_HANDLE(JSON_ELEMENT, HANDLE_NAME, RESULT) \
    if ((JSON_ELEMENT).contains(HANDLE_NAME)) \
    { \
        if ((JSON_ELEMENT)[HANDLE_NAME].is_number_unsigned()) \
            {(RESULT).node_index = (JSON_ELEMENT)[HANDLE_NAME].get<unsigned>(); \
            (RESULT).node_name = std::to_string((RESULT).node_index); } \
        else if ((JSON_ELEMENT)[HANDLE_NAME].is_string()) \
            (RESULT).node_name = (JSON_ELEMENT)[HANDLE_NAME].get<std::string>(); \
        else GLTF_CHECK(false);\
    }

#define glTF_PRCOESS_HANDLE_VEC(JSON_ELEMENT, VEC_NAME, RESULT) \
    if ((JSON_ELEMENT).contains(VEC_NAME)) \
    { \
        for (const auto& node_raw_data : (JSON_ELEMENT)[VEC_NAME])\
        { \
            glTFHandle handle; \
            if (node_raw_data.is_number_unsigned()) \
            { \
                handle.node_index = (node_raw_data.get<unsigned>()); \
            } \
            else if (node_raw_data.is_string())\
            { \
                handle.node_name = node_raw_data.get<std::string>(); \
            } \
            else {GLTF_CHECK(false);} \
            (RESULT).push_back(handle); \
        } \
    }

// Can not use name as handle name!
#define glTF_PROCESS_NAME_AND_HANDLE(JSON_ELEMENT, HANDLE_NAME, HANDLE_INDEX, RESULT) \
    glTF_PROCESS_SCALAR(JSON_ELEMENT, "name", std::string, (RESULT)->name); \
    (RESULT)->self_handle = glTFHandle((HANDLE_NAME), (HANDLE_INDEX)); \
    GLTF_CHECK(!(HANDLE_NAME).empty()); m_handleResolveMap[(HANDLE_NAME)] = (HANDLE_INDEX); (HANDLE_INDEX)++;

#define glTF_PROCESS_NODE_MESH(JSON_ELEMENT, RESULT) \
    glTFHandle mesh_handle; glTF_PROCESS_HANDLE(JSON_ELEMENT, "mesh", mesh_handle) if (mesh_handle.IsValid()) (RESULT)->meshes.push_back(mesh_handle);

#define glTF_PROCESS_NODE_MESHES(JSON_ELEMENT, RESULT) glTF_PRCOESS_HANDLE_VEC(JSON_ELEMENT, "meshes", (RESULT)->meshes)

#define glTF_PROCESS_NODE_CAMERA(JSON_ELEMENT, RESULT) glTF_PROCESS_HANDLE(JSON_ELEMENT, "camera", (RESULT)->camera)
#define glTF_PROCESS_NODE_CHILDREN(JSON_ELEMENT, RESULT) glTF_PRCOESS_HANDLE_VEC(JSON_ELEMENT, "children", (RESULT)->children)
#define glTF_PROCESS_NODE_NODES(JSON_ELEMENT, RESULT) glTF_PRCOESS_HANDLE_VEC(JSON_ELEMENT, "nodes", (RESULT))

#define glTF_PROCESS_PRIMITIVE_ATTRIBUTE(JSON_ELEMENT, ATTRIBUTE_NAME, RESULT) glTF_PROCESS_HANDLE(JSON_ELEMENT, #ATTRIBUTE_NAME, (RESULT)[glTF_Attribute_##ATTRIBUTE_NAME::attribute_type_id])
#define glTF_PROCESS_PRIMITIVE_INDEX(JSON_ELEMENT, RESULT) glTF_PROCESS_HANDLE(JSON_ELEMENT, "indices", RESULT)
#define glTF_PROCESS_PRIMITIVE_MODE(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "mode", glTF_Primitive_Mode, RESULT)

#define glTF_PROCESS_BUFFER_URI(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "uri", std::string, (RESULT)->uri)
#define glTF_PROCESS_BUFFER_BYTELENGTH(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "byteLength", unsigned, (RESULT)->byte_length)

#define glTF_PROCESS_BUFFERVIEW_BUFFER(JSON_ELEMENT, RESULT) glTF_PROCESS_HANDLE(JSON_ELEMENT, "buffer", (RESULT)->buffer)
#define glTF_PROCESS_BUFFERVIEW_BYTEOFFSET(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "byteOffset", unsigned, (RESULT)->byte_offset)
#define glTF_PROCESS_BUFFERVIEW_BYTELENGTH(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "byteLength", unsigned, (RESULT)->byte_length)
#define glTF_PROCESS_BUFFERVIEW_TARGET(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "target", glTF_BufferView_Target, (RESULT)->target)

#define glTF_PROCESS_ACCESSOR_COUNT(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "count", size_t, (RESULT)->count)
#define glTF_PROCESS_ACCESSOR_NORMALIZED(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "normalized", bool, (RESULT)->normalized)
#define glTF_PROCESS_ACCESSOR_BYTEOFFSET(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "byteOffset", int, (RESULT)->byte_offset)
#define glTF_PROCESS_ACCESSOR_BUFFERVIEW(JSON_ELEMENT, RESULT) glTF_PROCESS_HANDLE(JSON_ELEMENT, "bufferView", (RESULT)->buffer_view)

typedef std::uint64_t hash_t;  

constexpr hash_t prime = 0x100000001B3ull;  
constexpr hash_t basis = 0xCBF29CE484222325ull;  
  
hash_t hash_(char const* str)  
{  
    hash_t ret{basis};  
   
    while(*str){  
        ret ^= *str;  
        ret *= prime;  
        str++;  
    }  
   
    return ret;  
} 

constexpr hash_t hash_compile_time(const char* str, hash_t last_value = basis) 
{
    return *str ? hash_compile_time(str+1, (*str ^ last_value) * prime) : last_value;  
}

glTF_Accessor_Element_Type ParseAccessorElementType(const std::string& element_type_string)
{
    glTF_Accessor_Element_Type element_type = glTF_Accessor_Element_Type::EUnknown;
    switch (hash_compile_time(element_type_string.c_str()))
    {
    case hash_compile_time("SCALAR"):
        element_type = glTF_Accessor_Element_Type::EScalar;
        break;

    case hash_compile_time("VEC2"):
        element_type = glTF_Accessor_Element_Type::EVec2;
        break;

    case hash_compile_time("VEC3"):
        element_type = glTF_Accessor_Element_Type::EVec3;
        break;

    case hash_compile_time("VEC4"):
        element_type = glTF_Accessor_Element_Type::EVec4;
        break;

    case hash_compile_time("MAT2"):
        element_type = glTF_Accessor_Element_Type::EMat2;
        break;

    case hash_compile_time("MAT3"):
        element_type = glTF_Accessor_Element_Type::EMat3;
        break;

    case hash_compile_time("MAT4"):
        element_type = glTF_Accessor_Element_Type::EMat4;
        break;

    default:
        // Invalid type string
        break;
    }

    return element_type;
}

glTFLoader::glTFLoader()
= default;

bool glTFLoader::LoadFile(const std::string& file_path)
{
    std::ifstream glTF_file(file_path);
    if (glTF_file.bad())
    {
        return false;
    }
    
    nlohmann::json data = nlohmann::json::parse(glTF_file);
    decltype(glTFHandle::node_index) handle_index;

    std::map<decltype(glTFHandle::node_name), decltype(glTFHandle::node_index)> temporary_handle_resolve_map;
    
    // Parse nodes data
    handle_index = 0;
    for (const auto& [handle_name, raw_data] : data["nodes"].items())
    {
        std::unique_ptr<glTF_Element_Node> element = std::make_unique<glTF_Element_Node>();

        glTF_PROCESS_NAME_AND_HANDLE(raw_data, handle_name, handle_index, element)
        glTF_PROCESS_NODE_MESH(raw_data, element)
        glTF_PROCESS_NODE_MESHES(raw_data, element)
        glTF_PROCESS_NODE_CAMERA(raw_data, element)
        glTF_PROCESS_NODE_CHILDREN(raw_data, element)

        // Get Transform
        glTF_Transform transform;
        if (raw_data.contains("matrix"))
        {
            std::vector<float> matrix_data = raw_data["matrix"].get<std::vector<float>>();
            // TODO: @JACK check memcpy() can work for glm::mat4x4?
            GLTF_CHECK(matrix_data.size() == 16);
            memcpy(&transform.m_matrix, matrix_data.data(), matrix_data.size() * sizeof(float));
        }
        else if (raw_data.contains("scale"))
        {
            //TODO: 
        }
        else if (raw_data.contains("rotation"))
        {
            //TODO: 
        }
        else if (raw_data.contains("translation"))
        {
            //TODO: 
        }
        
        m_nodes.push_back(std::move(element));
    }

    // Parse meshes data
    handle_index = 0;
    for (const auto& [handle_name, raw_data] : data["meshes"].items())
    {
        std::unique_ptr<glTF_Element_Mesh> element = std::make_unique<glTF_Element_Mesh>();

        glTF_PROCESS_NAME_AND_HANDLE(raw_data, handle_name, handle_index, element)
        if (raw_data.contains("primitives"))
        {
            for (const auto& primitive_raw_data : raw_data["primitives"])
            {
                glTF_Primitive primitive;
                if (primitive_raw_data.contains("attributes"))
                {
                    glTF_PROCESS_PRIMITIVE_ATTRIBUTE(primitive_raw_data["attributes"], POSITION, primitive.attributes)
                    glTF_PROCESS_PRIMITIVE_ATTRIBUTE(primitive_raw_data["attributes"], NORMAL, primitive.attributes)
                    glTF_PROCESS_PRIMITIVE_ATTRIBUTE(primitive_raw_data["attributes"], TANGENT, primitive.attributes)
                }

                glTF_PROCESS_PRIMITIVE_INDEX(primitive_raw_data, primitive.indices)
                glTF_PROCESS_PRIMITIVE_MODE(primitive_raw_data, primitive.mode)

                // TODO: Handle material node

                element->primitives.push_back(primitive);
            }
        }

        m_meshes.push_back(std::move(element));
    }

    // Parse buffers data
    handle_index = 0;
    for (const auto& [handle_name, raw_data]  : data["buffers"].items())
    {
        std::unique_ptr<glTF_Element_Buffer> element = std::make_unique<glTF_Element_Buffer>();

        glTF_PROCESS_NAME_AND_HANDLE(raw_data, handle_name, handle_index, element)
        glTF_PROCESS_BUFFER_URI(raw_data, element)
        glTF_PROCESS_BUFFER_BYTELENGTH(raw_data, element)
        
        m_buffers.push_back(std::move(element));
    }

    // Parse buffer views data
    handle_index = 0;
    for (const auto& [handle_name, raw_data] : data["bufferViews"].items())
    {
        std::unique_ptr<glTF_Element_BufferView> element = std::make_unique<glTF_Element_BufferView>();
        
        glTF_PROCESS_NAME_AND_HANDLE(raw_data, handle_name, handle_index, element)
        glTF_PROCESS_BUFFERVIEW_BUFFER(raw_data, element)
        glTF_PROCESS_BUFFERVIEW_TARGET(raw_data, element)
        glTF_PROCESS_BUFFERVIEW_BYTELENGTH(raw_data, element)
        glTF_PROCESS_BUFFERVIEW_BYTEOFFSET(raw_data, element)
        
        m_bufferViews.push_back(std::move(element));
    }

    // Parse accessors data
    handle_index = 0;
    for (const auto& [handle_name, raw_data] : data["accessors"].items())
    {
        std::unique_ptr<glTF_Element_Accessor_Base> element = nullptr;
        // Resolve accessor component type first
        glTF_Accessor_Component_Type component_type = raw_data["componentType"].get<glTF_Accessor_Component_Type>();
        switch (component_type)
        {
        case EByte:
            element = std::make_unique<glTF_Element_Accessor_Byte>();
            break;
            
        case EUnsignedByte:
            element = std::make_unique<glTF_Element_Accessor_UByte>();
            break;
            
        case EShort:
            element = std::make_unique<glTF_Element_Accessor_Short>();
            break;
            
        case EUnsignedShort:
            element = std::make_unique<glTF_Element_Accessor_UShort>();
            break;
            
        case EUnsignedInt:
            element = std::make_unique<glTF_Element_Accessor_UInt>();
            break;
            
        case EFloat:
            element = std::make_unique<glTF_Element_Accessor_Float>();
            break;
            
        default:
            // Invalid component type
            GLTF_CHECK(false);
            break;
        }

        if (!element)
        {
            continue;
        }

        const std::string element_type_string = raw_data["type"].get<std::string>();
        glTF_Accessor_Element_Type element_type = ParseAccessorElementType(element_type_string);
        
        glTF_PROCESS_NAME_AND_HANDLE(raw_data, handle_name, handle_index, element)
        element->element_type = element_type;
        element->component_type = component_type;
        glTF_PROCESS_ACCESSOR_COUNT(raw_data, element)
        glTF_PROCESS_ACCESSOR_NORMALIZED(raw_data, element)
        glTF_PROCESS_ACCESSOR_BYTEOFFSET(raw_data, element)
        glTF_PROCESS_ACCESSOR_BUFFERVIEW(raw_data, element)

        m_accessors.push_back(std::move(element));
    }
    
    std::string directory;
    const size_t last_slash_idx = file_path.rfind('\\');
    if (std::string::npos != last_slash_idx)
    {
        directory = file_path.substr(0, last_slash_idx);
    }
    
    for (const auto& buffer : m_buffers)
    {
        const std::string uriFilePath = directory + "\\" + buffer->uri;
        std::ifstream uriFileStream(uriFilePath, std::ios::binary | std::ios::in | std::ios::ate);
        if (uriFileStream.bad())
        {
            GLTF_CHECK(false);
            continue;
        }

        const size_t fileSize = uriFileStream.tellg();
        GLTF_CHECK(fileSize == buffer->byte_length);
        
        uriFileStream.seekg(0, std::ios::beg);
        
        m_bufferDatas[buffer->self_handle] = std::make_unique<char[]>(fileSize);
        memset(m_bufferDatas[buffer->self_handle].get(), 0, fileSize);
        
        uriFileStream.read(m_bufferDatas[buffer->self_handle].get(), fileSize);
        uriFileStream.close();
    }

    handle_index = 0;
    for (const auto& [handle_name, raw_data] : data["scenes"].items())
    {
        std::unique_ptr<glTF_Element_Scene> element = std::make_unique<glTF_Element_Scene>();
        
        glTF_PROCESS_NAME_AND_HANDLE(raw_data, handle_name, handle_index, element)
        glTF_PROCESS_NODE_NODES(raw_data, element->root_nodes)
        
        m_scenes.push_back(std::move(element));
    }

    // Parse scene data
    if (data["scene"].is_number_unsigned())
    {
        default_scene = data["scene"].get<unsigned>();    
    }
    else if (data["scene"].is_string())
    {
        std::string scene_name = data["scene"].get<std::string>();
        
        // Find name in scenes array
        bool find_default_scene = false;
        for (size_t i = 0; i < m_scenes.size(); ++i)
        {
            if (m_scenes[i]->name == scene_name || m_scenes[i]->self_handle.node_name == scene_name)
            {
                default_scene = i;
                find_default_scene = true;
                break;
            }
        }

        GLTF_CHECK(find_default_scene);
    }

    // Process parent handle
    for (const auto& node : m_nodes)
    {
        for (const auto& child_index : node->children)
        {
            m_nodes[ResolveIndex(child_index)]->parent = node->self_handle;
        }
    }
    
    // TODO: @JACK Parse other types
    return true;
}

void glTFLoader::Print() const
{
    for (const auto& scene : m_scenes)
    {
        LOG_FORMAT("[DEBUG] Scene element handle: %d name: %s\n", scene->self_handle.node_index, scene->self_handle.node_name.c_str())
    }

    for (const auto& node : m_nodes)
    {
        LOG_FORMAT("[DEBUG] Node element handle: %d name: %s\n", node->self_handle.node_index, node->self_handle.node_name.c_str())
    }

    for (const auto& mesh : m_meshes)
    {
        LOG_FORMAT("[DEBUG] Mesh element handle: %d name: %s\n", mesh->self_handle.node_index, mesh->self_handle.node_name.c_str())
        for (const auto& primitive : mesh->primitives)
        {
            for (const auto& attribute: primitive.attributes)
            {
                const std::string attributeName = AttributeName(static_cast<glTF_Attribute>(attribute.first));
                LOG_FORMAT("[DEBUG] Mesh primitve contains %s\n", attributeName.c_str());
            }
        }
    }
}