#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

template <typename ConstantBufferType>
class glTFRenderInterfaceSingleConstantBuffer : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceSingleConstantBuffer(unsigned root_parameter_cbv_index, unsigned register_index)
        : m_root_parameter_cbv_index(root_parameter_cbv_index)
        , m_register_index(register_index)
    {
    }

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
    
    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager)
    {
        RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resource_manager.GetCommandListForRecord(),
        m_root_parameter_cbv_index, m_constant_gpu_data->GetGPUBufferHandle()))
    
        return true;
    }

    virtual bool ApplyRootSignature(IRHIRootSignature& rootSignature) const
    {
        return rootSignature.GetRootParameter(m_root_parameter_cbv_index).InitAsCBV(m_register_index);
    }

protected:
    unsigned m_root_parameter_cbv_index;
    unsigned m_register_index;
    std::shared_ptr<IRHIGPUBuffer> m_constant_gpu_data;
};
