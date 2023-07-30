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
    bool using_normal_mapping {false};
};

class glTFRenderPassInterfaceSceneMesh : public glTFRenderPassInterfaceBase
{
public:
    glTFRenderPassInterfaceSceneMesh(
        unsigned root_parameter_cbv_index,
        unsigned cbv_register_index,
        unsigned root_parameter_srv_index,
        unsigned srv_register_index,
        unsigned srv_max_count);
    
    virtual bool InitInterface(glTFRenderResourceManager& resourceManager) override;
    bool UpdateSceneMeshData(const ConstantBufferSceneMesh& data);
    bool ApplyInterface(glTFRenderResourceManager& resourceManager, unsigned meshIndex);

    bool SetupRootSignature(IRHIRootSignature& rootSignature) const;
    void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const;
    
private:
    unsigned m_root_parameter_cbv_index;
    unsigned m_cbv_register_index;

    unsigned m_root_parameter_srv_index;
    unsigned m_srv_register_index;
    unsigned m_max_srv_count;
    
    ConstantBufferSceneMesh m_scene_mesh_data;
    std::shared_ptr<IRHIGPUBuffer> m_scene_mesh_GPU_data;
};
