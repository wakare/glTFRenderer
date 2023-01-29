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

#define glTF_PRCOESS_VEC(JSON_ELEMENT, VEC_NAME, VEC_TYPE, RESULT) \
    if ((JSON_ELEMENT).contains(VEC_NAME)) \
    { \
        (RESULT) = (JSON_ELEMENT)[VEC_NAME].get<std::vector<VEC_TYPE>>(); \
    }

#define glTF_PROCESS_NAME(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "name", std::string, (RESULT)->name)
#define glTF_PROCESS_NODE_MESH(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "mesh", glTFHandle, (RESULT)->mesh)
#define glTF_PROCESS_NODE_CAMERA(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "camera", glTFHandle, (RESULT)->camera)
#define glTF_PROCESS_NODE_CHILDREN(JSON_ELEMENT, RESULT) glTF_PRCOESS_VEC(JSON_ELEMENT, "children", glTFHandle, (RESULT)->children)

#define glTF_PROCESS_BUFFER_URI(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "uri", std::string, (RESULT)->uri)
#define glTF_PROCESS_BUFFER_BYTELENGTH(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "byteLength", glTFHandle, (RESULT)->byte_length)

#define glTF_PROCESS_BUFFERVIEW_BUFFER(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "buffer", unsigned, (RESULT)->buffer)
#define glTF_PROCESS_BUFFERVIEW_BYTEOFFSET(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "byteOffset", unsigned, (RESULT)->byte_offset)
#define glTF_PROCESS_BUFFERVIEW_BYTELENGTH(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "bufferLength", unsigned, (RESULT)->byte_length)
#define glTF_PROCESS_BUFFERVIEW_TARGET(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "target", glTF_BufferView_Target, (RESULT)->target)

#define glTF_PROCESS_ACCESSOR_COUNT(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "count", size_t, (RESULT)->count)
#define glTF_PROCESS_ACCESSOR_NORMALIZED(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "normalized", bool, (RESULT)->normalized)
#define glTF_PROCESS_ACCESSOR_BYTEOFFSET(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "byteOffset", int, (RESULT)->byte_offset)
#define glTF_PROCESS_ACCESSOR_BUFFERVIEW(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "bufferView", int, (RESULT)->buffer_view)

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

bool glTFLoader::LoadFile(const char* file_path)
{
    std::ifstream glTF_file(file_path);
    if (glTF_file.bad())
    {
        return false;
    }
    
    nlohmann::json data = nlohmann::json::parse(glTF_file);
    glTFHandle handle_index = 0;

    // Parse scene data
    default_scene = data["scene"].get<unsigned>();

    handle_index = 0;
    for (const auto& raw_data : data["scenes"])
    {
        std::unique_ptr<glTF_Element_Scene> element = std::make_unique<glTF_Element_Scene>();
        element->self_handle = handle_index++;

        glTF_PROCESS_NAME(raw_data, element)
        glTF_PRCOESS_VEC(raw_data, "nodes", unsigned, element->root_nodes)

        m_scenes.push_back(std::move(element));
    }

    // Parse nodes data
    handle_index = 0;
    for (const auto& raw_data : data["nodes"])
    {
        std::unique_ptr<glTF_Element_Node> element = std::make_unique<glTF_Element_Node>();
        element->self_handle = handle_index++;

        glTF_PROCESS_NAME(raw_data, element)
        glTF_PROCESS_NODE_MESH(raw_data, element)
        glTF_PROCESS_NODE_CAMERA(raw_data, element)
        glTF_PROCESS_NODE_CHILDREN(raw_data, element)
        
        // Get Transform
        glTF_Transform transform;
        if (raw_data.contains("matrix"))
        {
            std::vector<float> matrix_data = raw_data["matrix"].get<std::vector<float>>();
            // TODO: @JACK check memcpy() can work for glm::mat4x4?
            GLTF_CHECK(matrix_data.size() == 16);
            memcpy(&transform.matrix, matrix_data.data(), matrix_data.size() * sizeof(float));
        }
        else if (raw_data.contains("scale"))
        {
            
        }
        else if (raw_data.contains("rotation"))
        {
            
        }
        else if (raw_data.contains("translation"))
        {
            
        }
        
        m_nodes.push_back(std::move(element));
    }

    // Process parent handle
    for (const auto& node : m_nodes)
    {
        for (const auto& child_index : node->children)
        {
            m_nodes[child_index]->parent = node->self_handle;
        }
    }

    // Parse buffers data
    handle_index = 0;
    for (const auto& raw_data : data["buffers"])
    {
        std::unique_ptr<glTF_Element_Buffer> element = std::make_unique<glTF_Element_Buffer>();
        element->self_handle = handle_index++;

        glTF_PROCESS_NAME(raw_data, element)
        glTF_PROCESS_BUFFER_URI(raw_data, element)
        glTF_PROCESS_BUFFER_BYTELENGTH(raw_data, element)
        
        m_buffers.push_back(std::move(element));
    }

    // Parse buffer views data
    handle_index = 0;
    for (const auto& raw_data : data["bufferViews"])
    {
        std::unique_ptr<glTF_Element_BufferView> element = std::make_unique<glTF_Element_BufferView>();
        element->self_handle = handle_index++;

        glTF_PROCESS_NAME(raw_data, element)
        glTF_PROCESS_BUFFERVIEW_BUFFER(raw_data, element)
        glTF_PROCESS_BUFFERVIEW_TARGET(raw_data, element)
        glTF_PROCESS_BUFFERVIEW_BYTELENGTH(raw_data, element)
        glTF_PROCESS_BUFFERVIEW_BYTEOFFSET(raw_data, element)
        
        m_bufferViews.push_back(std::move(element));
    }

    // Parse accessors data
    handle_index = 0;
    for (const auto& raw_data : data["accessors"])
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

        element->self_handle = handle_index++;
        
        const std::string element_type_string = raw_data["type"].get<std::string>();
        glTF_Accessor_Element_Type element_type = ParseAccessorElementType(element_type_string);
        
        
        glTF_PROCESS_NAME(raw_data, element)
        element->element_type = element_type;
        element->component_type = component_type;
        glTF_PROCESS_ACCESSOR_COUNT(raw_data, element)
        glTF_PROCESS_ACCESSOR_NORMALIZED(raw_data, element)
        glTF_PROCESS_ACCESSOR_BYTEOFFSET(raw_data, element)
        glTF_PROCESS_ACCESSOR_BUFFERVIEW(raw_data, element)

        m_accessors.push_back(std::move(element));
    }

    // TODO: @JACK Parse other types

    return true;
}

void glTFLoader::Print() const
{
    for (const auto& scene : m_scenes)
    {
        LOG_FORMAT_FLUSH("[DEBUG] Scene element handle: %d\n", scene->self_handle);
    }

    for (const auto& node : m_nodes)
    {
        LOG_FORMAT_FLUSH("[DEBUG] Node element handle: %d\n", node->self_handle);
    }
}
