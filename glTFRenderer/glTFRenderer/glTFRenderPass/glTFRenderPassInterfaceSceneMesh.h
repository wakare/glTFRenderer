#pragma once
#include "glTFRenderPassInterfaceBase.h"
#include "glm/glm/mat4x4.hpp"

class IRHIGPUBuffer;

struct ConstantBufferSceneMesh
{
    glm::mat4 worldMat {glm::mat4{1.0f}};
};

class glTFRenderPassInterfaceSceneMesh : public glTFRenderPassInterfaceBase
{
public:
    virtual bool InitInterface(glTFRenderResourceManager& resourceManager) override;
    bool UpdateSceneMeshData(const ConstantBufferSceneMesh& data);
    bool ApplyInterface(glTFRenderResourceManager& resourceManager, unsigned meshIndex, unsigned rootParameterSlotIndex);

private:
    ConstantBufferSceneMesh m_sceneMeshData;
    std::shared_ptr<IRHIGPUBuffer> m_sceneMeshGPUData;
};
