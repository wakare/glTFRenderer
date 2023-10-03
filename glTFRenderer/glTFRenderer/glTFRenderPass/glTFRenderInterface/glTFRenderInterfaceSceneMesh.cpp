#include "glTFRenderInterfaceSceneMesh.h"

void glTFRenderInterfaceSceneMesh::UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", m_allocation.register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_CBV_INDEX", registerIndexValue);
    LOG_FORMAT("[INFO] Add shader preDefine %s, %s\n", "SCENE_MESH_REGISTER_CBV_INDEX", registerIndexValue)
}

void glTFRenderInterfaceSceneMeshMaterial::UpdateShaderCompileDefine(
    RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
        
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_allocation.register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ZERO", registerIndexValue);

    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_allocation.register_index + 1);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ONE", registerIndexValue);
}