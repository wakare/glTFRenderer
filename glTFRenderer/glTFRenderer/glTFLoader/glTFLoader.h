#pragma once
#include <memory>
#include <vector>

#include "glTFElementCommon.h"

class glTFLoader
{
    friend class glTFSceneGraph;
    
public:
    glTFLoader();
    
    bool LoadFile(const std::string& file_path);
	const std::string& GetSceneFileDirectory() const;
    
    void Print() const;

    glTFHandle::HandleIndexType ResolveIndex(const glTFHandle& handle) const
    {
        if (handle.node_index == glTFHandle::glTF_ELEMENT_INVALID_HANDLE)
        {
            // find index from util map data
            auto it = m_handleResolveMap.find(handle.node_name);
            GLTF_CHECK(it != m_handleResolveMap.end());

            return it->second;
        }

        return handle.node_index;
    }
    
private:
	std::string m_scene_file_directory;
    
    unsigned m_default_scene {};
    std::vector<std::unique_ptr<glTF_Element_Scene>>            m_scenes;
    std::vector<std::unique_ptr<glTF_Element_Node>>             m_nodes;
    std::vector<std::unique_ptr<glTF_Element_Mesh>>             m_meshes;
    std::vector<std::unique_ptr<glTF_Element_Image>>			m_images;
    std::vector<std::unique_ptr<glTF_Element_Texture>>			m_textures;
    std::vector<std::unique_ptr<glTF_Element_Sampler>>			m_samplers;
    std::vector<std::unique_ptr<glTF_Element_Material>>			m_materials;
    std::vector<std::unique_ptr<glTF_Element_Buffer>>           m_buffers;
    std::vector<std::unique_ptr<glTF_Element_BufferView>>       m_bufferViews;
    std::vector<std::unique_ptr<glTF_Element_Accessor_Base>>    m_accessors;

    std::map<glTFHandle, std::unique_ptr<char[]>>               m_buffer_data;

    std::map<glTFHandle::HandleNameType, glTFHandle::HandleIndexType> m_handleResolveMap;
};