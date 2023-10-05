#pragma once
#include "glTFRenderInterfaceBase.h"

struct SceneMeshStartIndexInfo
{
    inline static std::string Name = "SceneMeshStartIndexInfo_REGISTER_SRV_INDEX";
    unsigned start_index;
};

struct SceneMeshIndexInfo
{
    inline static std::string Name = "SceneMeshIndexInfo_REGISTER_SRV_INDEX";
    unsigned vertex_index;
};

struct SceneMeshVertexInfo
{
    inline static std::string Name = "SceneMeshVertexInfo_REGISTER_SRV_INDEX";
    float uv[2];
};

class glTFRenderInterfaceSceneMeshInfo : public glTFRenderInterfaceBaseWithDefaultImpl
{
public:
    glTFRenderInterfaceSceneMeshInfo();

    bool UpdateSceneMeshData(const glTFRenderMeshManager& mesh_manager);
};
