#include "glTFRenderInterfaceSceneMesh.h"

glTFRenderInterfaceSceneMeshMaterial::glTFRenderInterfaceSceneMeshMaterial()
{
    SetSRVRegisterNames({"SCENE_MESH_REGISTER_SRV_INDEX_ZERO", "SCENE_MESH_REGISTER_SRV_INDEX_ONE"});
}
