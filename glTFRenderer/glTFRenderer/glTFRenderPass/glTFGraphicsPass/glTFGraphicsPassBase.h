#pragma once
#include "glTFRenderPass/glTFRenderPassBase.h"

class glTFMaterialBase;

class glTFGraphicsPassBase : public glTFRenderPassBase
{
public:
    glTFGraphicsPassBase();

    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    IRHIGraphicsPipelineStateObject& GetGraphicsPipelineStateObject() const;
    
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout(glTFRenderResourceManager& resource_manager) = 0;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual PipelineType GetPipelineType() const override {return PipelineType::Graphics; }
};