#pragma once
#include <memory>

#include "glm/glm/mat4x4.hpp"
#include "glTFRenderPassInterfaceBase.h"
#include "../glTFRHI/RHIInterface/IRHIRootSignature.h"
#include "../glTFRHI/RHIInterface/IRHIShader.h"

class IRHIGPUBuffer;

struct ConstantBufferSceneView
{
    glm::mat4 viewProjection {glm::mat4{1.0f}};
    glm::mat4 inverseViewMatrix {glm::mat4{1.0f}};
    glm::mat4 inverseProjectionMatrix {glm::mat4{1.0f}};
};

// Scene view interface provides constant buffer for view projection matrix
class glTFRenderPassInterfaceSceneView : public glTFRenderPassInterfaceBase
{
public:
    glTFRenderPassInterfaceSceneView(unsigned rootParameterIndex, unsigned registerIndex);
    
    virtual bool InitInterface(glTFRenderResourceManager& resourceManager) override;
    bool UpdateSceneViewData(const ConstantBufferSceneView& data);
    bool ApplyInterface(glTFRenderResourceManager& resourceManager, unsigned rootParameterSlotIndex);

    bool SetupRootSignature(IRHIRootSignature& rootSignature) const;
    void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const;
    
private:
    unsigned m_rootParameterIndex;
    unsigned m_registerIndex;
    ConstantBufferSceneView m_sceneViewData;
    std::shared_ptr<IRHIGPUBuffer> m_sceneViewGPUData;
};
