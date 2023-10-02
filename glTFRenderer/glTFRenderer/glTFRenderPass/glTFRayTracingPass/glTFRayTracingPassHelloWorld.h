#pragma once
#include "glTFRayTracingPassBase.h"
#include "glTFRayTracingPassWithMesh.h"
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"
#include "glTFRHI/RHIInterface/IRHIShaderTable.h"

class glTFRayTracingPassHelloWorld : public glTFRayTracingPassWithMesh
{
public:
    glTFRayTracingPassHelloWorld();

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual IRHIShaderTable& GetShaderTable() const override;
    virtual TraceCount GetTraceCount() const override;
    
protected:
    virtual size_t GetRootSignatureParameterCount() override;
    virtual size_t GetRootSignatureSamplerCount() override;
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

private:
    bool BuildAS(glTFRenderResourceManager& resource_manager);
    
    std::shared_ptr<IRHIShaderTable> m_shader_table;
    std::shared_ptr<IRHIRenderTarget> m_raytracing_output;
    std::shared_ptr<IRHIRayTracingAS> m_raytracing_as;
    TraceCount m_trace_count;
};
