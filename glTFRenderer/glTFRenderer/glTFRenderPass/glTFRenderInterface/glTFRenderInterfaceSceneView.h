#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFScene/glTFSceneView.h"

class glTFRenderInterfaceSceneView : public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneView>
{
public:
    glTFRenderInterfaceSceneView();

    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
};