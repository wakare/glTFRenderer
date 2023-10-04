#include "glTFRenderInterfaceSceneMesh.h"

void glTFRenderInterfaceSceneMeshMaterial::UpdateShaderCompileDefine(
    RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
        
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_allocation.register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ZERO", registerIndexValue);

    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_allocation.register_index + 1);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ONE", registerIndexValue);
}