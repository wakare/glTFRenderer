#pragma once
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"
#include "glTFUtils/glTFUtils.h"

class Dx12RayTracingAS : public IRHIRayTracingAS
{
public:
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIGPUBuffer& vertex_buffer, IRHIGPUBuffer& index_buffer) override;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const override;

private:
    ComPtr<ID3D12Resource> m_TLAS;
    ComPtr<ID3D12Resource> m_BLAS;
};
