#pragma once

#include "glTFRenderPassBase.h"
#include "glTFRHI/RHIInterface/IRHIShaderTable.h"

struct TraceCount
{
    unsigned X;
    unsigned Y;
    unsigned Z;
};

class glTFRayTracingPassBase : public glTFRenderPassBase
{
public:
    virtual ~glTFRayTracingPassBase() override = default;
    
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual TraceCount GetTraceCount() const = 0;
    virtual IRHIShaderTable& GetShaderTable() const = 0;
    
protected:
    IRHIRayTracingPipelineStateObject& GetRayTracingPipelineStateObject() const;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual PipelineType GetPipelineType() const override {return PipelineType::RayTracing; }
};
