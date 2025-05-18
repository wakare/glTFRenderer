﻿#pragma once
#include <d3d12.h>

#include "IRHIRayTracingAS.h"
#include "RendererCommon.h"
#include "IRHIMemoryManager.h"

class IRHIBufferDescriptorAllocation;

class DX12RayTracingAS : public IRHIRayTracingAS
{
public:
    DX12RayTracingAS();
    
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager&
                                  memory_manager) override;
    virtual const IRHIDescriptorAllocation& GetTLASDescriptorSRV() const override;

    const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& GetInstanceDesc() const;
    
private:
    std::vector<std::shared_ptr<IRHIBufferAllocation>> m_BLASes;
    std::vector<std::shared_ptr<IRHIBufferAllocation>> m_BLAS_scratch_buffers;
    
    std::shared_ptr<IRHIBufferAllocation> m_TLAS;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_TLAS_descriptor_allocation;
    std::shared_ptr<IRHIBufferAllocation> m_scratch_buffer;
    std::shared_ptr<IRHIBufferAllocation> m_upload_buffer;
    
    std::vector<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO> m_blas_prebuild_infos;
    std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS> m_blas_build_inputs;
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_blas_geometry_descs;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_instance_descs;
};
