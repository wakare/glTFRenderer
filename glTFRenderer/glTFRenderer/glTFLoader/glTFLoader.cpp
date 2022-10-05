#include "glTFLoader.h"

#include <fstream>
#include <iostream>

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
#define glTF_PROCESS_MESH(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "mesh", glTFHandle, (RESULT)->mesh)
#define glTF_PROCESS_CAMERA(JSON_ELEMENT, RESULT) glTF_PROCESS_SCALAR(JSON_ELEMENT, "camera", glTFHandle, (RESULT)->camera)
#define glTF_PROCESS_CHILDREN(JSON_ELEMENT, RESULT) glTF_PRCOESS_VEC(JSON_ELEMENT, "children", glTFHandle, (RESULT)->children)
    

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
        glTF_PROCESS_MESH(raw_data, element)
        glTF_PROCESS_CAMERA(raw_data, element)
        glTF_PROCESS_CHILDREN(raw_data, element)
        
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
    
}

void glTFLoader::Print() const
{
    for (const auto& scene : m_scenes)
    {
        std::cout << "[DEBUG] Scene element handle: " << scene->self_handle << std::endl;
    }

    for (const auto& node : m_nodes)
    {
        std::cout << "[DEBUG] Node element handle: " << node->self_handle << std::endl;
    }
}
