#pragma once
#include "glTFRayTracingPassBase.h"
#include "glTFRayTracingPassWithMesh.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"
#include "glTFRHI/RHIInterface/IRHIShaderTable.h"

struct RWTextureResourceWithBackBuffer
{
    RWTextureResourceWithBackBuffer(std::string output_register_name, std::string back_register_name);
    
    bool CreateResource(glTFRenderResourceManager& resource_manager, const IRHIRenderTargetDesc& desc);
    bool CreateDescriptors(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& main_descriptor);
    bool RegisterSignature(IRHIRootSignatureHelper& root_signature);
    bool AddShaderMacros(RHIShaderPreDefineMacros& macros);
    bool BindRootParameter(glTFRenderResourceManager& resource_manager);
    bool CopyToBackBuffer(glTFRenderResourceManager& resource_manager);

protected:
    std::string m_output_register_name;
    std::string m_back_register_name;
    
    std::string GetOutputBufferResourceName() const;
    std::string GetBackBufferResourceName() const;
    
    IRHIRenderTargetDesc m_texture_desc;
    
    std::shared_ptr<IRHIRenderTarget> m_writable_buffer;
    std::shared_ptr<IRHIRenderTarget> m_back_buffer;

    RHIGPUDescriptorHandle m_writable_buffer_handle;
    RHIGPUDescriptorHandle m_back_buffer_handle;
    
    RootSignatureAllocation m_writable_buffer_allocation;
    RootSignatureAllocation m_back_buffer_allocation;
};

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
    
    RHIGPUDescriptorHandle m_output_handle;
    
    TraceCount m_trace_count;

    RootSignatureAllocation m_output_allocation;
    RootSignatureAllocation m_raytracing_as_allocation;
    
    IRHIRootSignatureHelper m_local_rs;
    RootSignatureAllocation m_local_constant_allocation;

    RWTextureResourceWithBackBuffer m_accumulation_resource;
    RWTextureResourceWithBackBuffer m_custom_resource;
    
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
