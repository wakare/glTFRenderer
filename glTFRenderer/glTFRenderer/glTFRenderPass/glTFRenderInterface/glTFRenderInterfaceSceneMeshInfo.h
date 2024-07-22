#pragma once
#include "glTFRenderInterfaceBase.h"

ALIGN_FOR_CBV_STRUCT struct SceneMeshDataOffsetInfo
{
    inline static std::string Name = "SceneMeshDataOffsetInfo_REGISTER_SRV_INDEX";
    unsigned start_face_index;
    unsigned material_index;
    unsigned start_vertex_index;
};

ALIGN_FOR_CBV_STRUCT struct SceneMeshFaceInfo
{
    inline static std::string Name = "SceneMeshFaceInfo_REGISTER_SRV_INDEX";
    unsigned vertex_index[3];
};

ALIGN_FOR_CBV_STRUCT struct SceneMeshVertexInfo
{
    inline static std::string Name = "SceneMeshVertexInfo_REGISTER_SRV_INDEX";

    float position[4];
    float normal[4];
    float tangent[4];
    float uv[4];
};

class glTFRenderMeshManager;
class glTFRenderInterfaceSceneMeshInfo : public glTFRenderInterfaceBaseWithDefaultImpl
{
public:
    glTFRenderInterfaceSceneMeshInfo();

    bool UpdateSceneMeshData(glTFRenderResourceManager& resource_manager, const glTFRenderMeshManager& mesh_manager);
};
