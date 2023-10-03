#include "glTFRenderInterfaceSceneView.h"

void glTFRenderInterfaceSceneView::UpdateShaderCompileDefine(
    RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", m_allocation.register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_VIEW_REGISTER_INDEX", registerIndexValue);
    LOG_FORMAT("[INFO] Add shader preDefine %s, %s\n", "SCENE_VIEW_REGISTER_INDEX", registerIndexValue)
}