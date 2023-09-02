#pragma once
#include "glTFRenderPass/glTFRenderPassBase.h"

class glTFMaterialBase;

class glTFGraphicsPassBase : public glTFRenderPassBase
{
public:
    glTFGraphicsPassBase();

    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    
    // Which material should process within render pass
    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material);
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) = 0;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resourceManager) {return true; }

    virtual std::shared_ptr<IRHIPipelineStateObject> GetPSO() const override;
    
protected:
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() = 0;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    std::shared_ptr<IRHIGraphicsPipelineStateObject> m_pipeline_state_object;
};
