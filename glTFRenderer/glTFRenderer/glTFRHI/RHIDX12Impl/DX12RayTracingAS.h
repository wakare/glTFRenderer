#pragma once
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"
#include "glTFUtils/glTFUtils.h"

class DX12RayTracingAS : public IRHIRayTracingAS
{
public:
    DX12RayTracingAS();
    
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIVertexBuffer& vertex_buffer, IRHIIndexBuffer& index_buffer) override;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const override;

private:
    std::shared_ptr<IRHIGPUBuffer> m_TLAS;
    std::shared_ptr<IRHIGPUBuffer> m_BLAS;
    std::shared_ptr<IRHIGPUBuffer> m_scratch_buffer;
    std::shared_ptr<IRHIGPUBuffer> m_upload_buffer;
};
