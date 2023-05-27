#pragma once
#include <memory>

#include "glTFRenderPassInterfaceBase.h"
#include "glm/glm/mat4x4.hpp"

class IRHIGPUBuffer;

struct ConstantBufferSceneView
{
    glm::mat4 viewProjection {glm::mat4{1.0f}};
};

// Scene view interface provides constant buffer for view projection matrix
class glTFRenderPassInterfaceSceneView : public glTFRenderPassInterfaceBase
{
public:
    virtual bool InitInterface(glTFRenderResourceManager& resourceManager) override;
    bool UpdateSceneViewData(const ConstantBufferSceneView& data);
    bool ApplyInterface(glTFRenderResourceManager& resourceManager, unsigned rootParameterSlotIndex);

private:
    ConstantBufferSceneView m_sceneViewData;
    std::shared_ptr<IRHIGPUBuffer> m_sceneViewGPUData;
};
