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

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    
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
    std::shared_ptr<IRHIRenderTarget> m_screen_uv_offset_output;
    
    RHIGPUDescriptorHandle m_output_handle;
    RHIGPUDescriptorHandle m_screen_uv_offset_handle;
    
    TraceCount m_trace_count;

    RootSignatureAllocation m_output_allocation;
    RootSignatureAllocation m_raytracing_as_allocation;
    RootSignatureAllocation m_screen_uv_offset_allocation;
    
    IRHIRootSignatureHelper m_local_rs;
    RootSignatureAllocation m_local_constant_allocation;
    
    bool m_material_uploaded;
    
protected:
    // Ray function names
    std::string m_raygen_name;
    
    std::string m_primary_ray_closest_hit_name;
    std::string m_primary_ray_miss_name;
    std::string m_primary_ray_hit_group_name;
    
    std::string m_shadow_ray_miss_name;
    std::string m_shadow_ray_hit_group_name;
};
