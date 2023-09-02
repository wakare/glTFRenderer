#include "glTFRenderInterfaceSceneMesh.h"

glTFRenderInterfaceSceneMesh::glTFRenderInterfaceSceneMesh(unsigned root_parameter_cbv_index, unsigned register_index)
    : glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneMesh>(root_parameter_cbv_index, register_index)
{
}

void glTFRenderInterfaceSceneMesh::UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", m_register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_CBV_INDEX", registerIndexValue);
    LOG_FORMAT("[INFO] Add shader preDefine %s, %s\n", "SCENE_MESH_REGISTER_CBV_INDEX", registerIndexValue);
}

glTFRenderInterfaceSceneMeshMaterial::glTFRenderInterfaceSceneMeshMaterial(
    unsigned root_parameter_srv_index,
    unsigned srv_register_index,
    unsigned max_srv_count)
    : glTFRenderInterfaceShaderResourceView(root_parameter_srv_index, srv_register_index, max_srv_count)
{
    
}

void glTFRenderInterfaceSceneMeshMaterial::UpdateShaderCompileDefine(
    RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
        
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_srv_register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ZERO", registerIndexValue);

    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_srv_register_index + 1);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ONE", registerIndexValue);
}
