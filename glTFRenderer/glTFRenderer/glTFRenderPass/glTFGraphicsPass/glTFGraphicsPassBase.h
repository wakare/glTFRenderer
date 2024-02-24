#pragma once
#include "glTFRenderPass/glTFRenderPassBase.h"

class glTFMaterialBase;

class glTFGraphicsPassBase : public glTFRenderPassBase
{
public:
    glTFGraphicsPassBase();
    virtual ~glTFGraphicsPassBase() override = default;
    
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    IRHIGraphicsPipelineStateObject& GetGraphicsPipelineStateObject() const;
    
    virtual IRHICullMode GetCullMode();
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual PipelineType GetPipelineType() const override {return PipelineType::Graphics; }
};