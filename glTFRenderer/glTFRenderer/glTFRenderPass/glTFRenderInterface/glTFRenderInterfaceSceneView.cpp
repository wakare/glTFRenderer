#include "glTFRenderInterfaceSceneView.h"

glTFRenderInterfaceSceneView::glTFRenderInterfaceSceneView(unsigned root_parameter_cbv_index,
    unsigned register_index)
        : glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneView>(root_parameter_cbv_index, register_index)
{
}

void glTFRenderInterfaceSceneView::UpdateShaderCompileDefine(
    RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", m_register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_VIEW_REGISTER_INDEX", registerIndexValue);
    LOG_FORMAT("[INFO] Add shader preDefine %s, %s\n", "SCENE_VIEW_REGISTER_INDEX", registerIndexValue);
}