#pragma once
#include <memory>
#include <vector>

#include "glTFElementCommon.h"

class glTFLoader
{
    friend class glTFWindow;
public:
    glTFLoader();
    bool LoadFile(const std::string& file_path);
    void Print() const;
    
private:
    unsigned default_scene{};
    std::vector<std::unique_ptr<glTF_Element_Scene>>            m_scenes;
    std::vector<std::unique_ptr<glTF_Element_Node>>             m_nodes;
    std::vector<std::unique_ptr<glTF_Element_Mesh>>             m_meshes;
    std::vector<std::unique_ptr<glTF_Element_Buffer>>           m_buffers;
    std::vector<std::unique_ptr<glTF_Element_BufferView>>       m_bufferViews;
    std::vector<std::unique_ptr<glTF_Element_Accessor_Base>>    m_accessors;

    std::map<glTFHandle::HandleIndexType, std::unique_ptr<char[]>>               m_bufferDatas;
};