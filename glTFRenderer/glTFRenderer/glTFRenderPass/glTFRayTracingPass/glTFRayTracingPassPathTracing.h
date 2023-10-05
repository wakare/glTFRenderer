#pragma once
#include "glTFRayTracingPassBase.h"
#include "glTFRayTracingPassWithMesh.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"
#include "glTFRHI/RHIInterface/IRHIShaderTable.h"

class glTFRayTracingPassPathTracing : public glTFRayTracingPassWithMesh
{
public:
    glTFRayTracingPassPathTracing();

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual IRHIShaderTable& GetShaderTable() const override;
    virtual TraceCount GetTraceCount() const override;
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

private:
    bool UpdateAS(glTFRenderResourceManager& resource_manager);
    bool BuildAS(glTFRenderResourceManager& resource_manager);
    
    std::shared_ptr<IRHIShaderTable> m_shader_table;
    std::shared_ptr<IRHIRenderTarget> m_raytracing_output;
    std::shared_ptr<IRHIRayTracingAS> m_raytracing_as;
    TraceCount m_trace_count;

    RootSignatureAllocation m_output_allocation;
    RootSignatureAllocation m_raytracing_as_allocation;
    bool m_material_uploaded;
};
