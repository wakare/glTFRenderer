#pragma once
#include <d3d12.h>

#include "glTFRenderPass/glTFRenderMeshManager.h"
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"
#include "RendererCommon.h"

class DX12RayTracingAS : public IRHIRayTracingAS
{
public:
    DX12RayTracingAS();
    
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const glTFRenderMeshManager& mesh_manager) override;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const override;

    const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& GetInstanceDesc() const;
    
private:
    std::vector<std::shared_ptr<IRHIGPUBuffer>> m_BLASes;
    std::vector<std::shared_ptr<IRHIGPUBuffer>> m_BLAS_scratch_buffers;
    
    std::shared_ptr<IRHIGPUBuffer> m_TLAS;
    std::shared_ptr<IRHIGPUBuffer> m_scratch_buffer;
    std::shared_ptr<IRHIGPUBuffer> m_upload_buffer;
    
    std::vector<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO> m_blas_prebuild_infos;
    std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS> m_blas_build_inputs;
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_blas_geometry_descs;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_instance_descs;
};
