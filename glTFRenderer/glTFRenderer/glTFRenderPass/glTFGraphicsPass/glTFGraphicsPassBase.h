#pragma once
#include "glTFRenderPass/glTFRenderPassBase.h"

class IRHIGraphicsPipelineStateObject;
class glTFMaterialBase;

class glTFGraphicsPassBase : public glTFRenderPassBase
{
public:
    glTFGraphicsPassBase();
    virtual ~glTFGraphicsPassBase() override = default;
    
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const;
    IRHIGraphicsPipelineStateObject& GetGraphicsPipelineStateObject() const;
    
    virtual RHICullMode GetCullMode();
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual RHIPipelineType GetPipelineType() const override {return RHIPipelineType::Graphics; }
    
    RHIBeginRenderingInfo m_begin_rendering_info;
};