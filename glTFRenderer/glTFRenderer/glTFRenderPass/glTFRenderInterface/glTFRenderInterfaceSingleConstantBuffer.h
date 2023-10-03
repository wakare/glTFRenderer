#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

template <typename ConstantBufferType>
class glTFRenderInterfaceSingleConstantBuffer : public glTFRenderInterfaceBase
{
public:
    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override
    {
        m_constant_gpu_data = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        // TODO: 64k is enough ??
        RETURN_IF_FALSE(m_constant_gpu_data->InitGPUBuffer(resource_manager.GetDevice(),
            {
                L"GPUBuffer_SingleConstantBuffer",
                static_cast<size_t>(64 * 1024),
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer
            }))
    
        return true;
    }

    virtual bool UpdateCPUBuffer(const void* data, size_t size)
    {
        return m_constant_gpu_data->UploadBufferFromCPU(data, 0, size);
    }
    
    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline)
    {
        RETURN_IF_FALSE(RHIUtils::Instance().SetCBVToRootParameterSlot(resource_manager.GetCommandListForRecord(),
        m_allocation.parameter_index, m_constant_gpu_data->GetGPUBufferHandle(), isGraphicsPipeline))
    
        return true;
    }

    virtual bool ApplyRootSignature(IRHIRootSignatureHelper& rootSignature)
    {
        return rootSignature.AddCBVRootParameter("GPUBuffer_SingleConstantBuffer", m_allocation);
    }

protected:
    RootSignatureAllocation m_allocation;
    std::shared_ptr<IRHIGPUBuffer> m_constant_gpu_data;
};