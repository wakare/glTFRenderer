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

    virtual bool UpdateConstantBuffer(const ConstantBufferType& data)
    {
        m_constant_buffer = data;
        return true;
    }
    
    virtual bool ApplyInterface(glTFRenderResourceManager& resourceManager)
    {
        RETURN_IF_FALSE(m_constant_gpu_data->UploadBufferFromCPU(&m_constant_buffer, 0, sizeof(m_constant_buffer)))
        RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandListForRecord(),
        m_root_parameter_cbv_index, m_constant_gpu_data->GetGPUBufferHandle()))
    
        return true;
    }

    virtual bool SetupRootSignature(IRHIRootSignature& rootSignature) const
    {
        return rootSignature.GetRootParameter(m_root_parameter_cbv_index).InitAsCBV(m_register_index);
    }
    
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const = 0;

protected:
    unsigned m_root_parameter_cbv_index;
    unsigned m_register_index;
    ConstantBufferType m_constant_buffer;
    std::shared_ptr<IRHIGPUBuffer> m_constant_gpu_data;
};

#define IMPLEMENT_SINGLE_CONSTANT_BUFFER_TYPE(Name) \
    typedef glTFRenderInterfaceSingleConstantBuffer<Name> glTFRenderInterface##Name;
