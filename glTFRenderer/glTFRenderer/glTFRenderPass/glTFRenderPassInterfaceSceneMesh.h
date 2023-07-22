#pragma once
#include "glTFRenderPassInterfaceBase.h"
#include "glm/glm/mat4x4.hpp"

struct RHIShaderPreDefineMacros;
class IRHIRootSignature;
class IRHIGPUBuffer;

struct ConstantBufferSceneMesh
{
    glm::mat4 worldMat {glm::mat4{1.0f}};
    glm::mat4 transInvWorldMat {glm::mat4{1.0f}};
};

class glTFRenderPassInterfaceSceneMesh : public glTFRenderPassInterfaceBase
{
public:
    glTFRenderPassInterfaceSceneMesh(unsigned rootParameterIndex, unsigned registerIndex);
    
    virtual bool InitInterface(glTFRenderResourceManager& resourceManager) override;
    bool UpdateSceneMeshData(const ConstantBufferSceneMesh& data);
    bool ApplyInterface(glTFRenderResourceManager& resourceManager, unsigned meshIndex, unsigned rootParameterSlotIndex);

    bool SetupRootSignature(IRHIRootSignature& rootSignature) const;
    void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const;
    
private:
    unsigned m_rootParameterIndex;
    unsigned m_registerIndex;
    
    ConstantBufferSceneMesh m_scene_mesh_data;
    std::shared_ptr<IRHIGPUBuffer> m_scene_mesh_GPU_data;
};
