#pragma once
#include "glTFRenderPass/glTFRenderPassBase.h"

class glTFMaterialBase;

class glTFGraphicsPassBase : public glTFRenderPassBase
{
public:
    glTFGraphicsPassBase();

    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual std::shared_ptr<IRHIPipelineStateObject> GetPSO() const override;
    
protected:
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() = 0;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    std::shared_ptr<IRHIGraphicsPipelineStateObject> m_pipeline_state_object;
};