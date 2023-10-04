#include "IRHIRootSignatureHelper.h"

#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFUtils/glTFUtils.h"

bool IRHIRootSignatureHelper::AddRootParameterWithRegisterCount(const std::string& parameter_name, RHIRootParameterType type,
                                                                unsigned register_count, unsigned constant_value, RHIRootParameterDescriptorRangeType table_type, bool is_bindless, RootSignatureAllocation& out_allocation)
{
    RHIShaderRegisterType register_type = RHIShaderRegisterType::Unknown;
    switch (type) {
    case RHIRootParameterType::Constant:
        register_type = RHIShaderRegisterType::b;
        break;
    case RHIRootParameterType::CBV:
        register_type = RHIShaderRegisterType::b;
        break;
    case RHIRootParameterType::SRV:
        register_type = RHIShaderRegisterType::t;
        break;
    case RHIRootParameterType::UAV:
        register_type = RHIShaderRegisterType::u;
        break;
    case RHIRootParameterType::DescriptorTable:
        {
            switch (table_type) {
            case RHIRootParameterDescriptorRangeType::CBV:
                register_type = RHIShaderRegisterType::b;
                break;
            case RHIRootParameterDescriptorRangeType::SRV:
                register_type = RHIShaderRegisterType::t;
                break;
            case RHIRootParameterDescriptorRangeType::UAV:
                register_type = RHIShaderRegisterType::u;
                break;
            case RHIRootParameterDescriptorRangeType::Unknown:
                GLTF_CHECK(false);
            default: ;
            }
        }
        break;
    case RHIRootParameterType::Unknown:
    default: GLTF_CHECK(false);
    }
    
    unsigned register_start = m_layout.last_register_index[register_type];
    unsigned register_end = register_start + register_count;
    
    RootSignatureParameterElement element;
    element.name = parameter_name;
    element.register_range = {register_start, register_end};
    element.parameter_index = m_layout.last_parameter_index++;
    if (type == RHIRootParameterType::Constant)
    {
        element.constant_value = constant_value;
    }
    if (type == RHIRootParameterType::DescriptorTable)
    {
        element.table_type = table_type;
        element.is_bindless = is_bindless;
    }
    
    m_layout.parameter_elements[type].push_back(element);

    out_allocation.parameter_index = element.parameter_index;
    out_allocation.register_index = register_start;
    m_layout.last_register_index[register_type] += register_count;
    
    return true;
}

IRHIRootSignatureHelper::IRHIRootSignatureHelper()
    : m_usage(RHIRootSignatureUsage::None)
{
}

bool IRHIRootSignatureHelper::AddConstantRootParameter(const std::string& parameter_name, unsigned constant_value, RootSignatureAllocation& out_allocation)
{
    return AddRootParameterWithRegisterCount(parameter_name, RHIRootParameterType::Constant, 1, constant_value, RHIRootParameterDescriptorRangeType::Unknown, false, out_allocation);
}

bool IRHIRootSignatureHelper::AddCBVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation)
{
    return AddRootParameterWithRegisterCount(parameter_name, RHIRootParameterType::CBV, 1, 0, RHIRootParameterDescriptorRangeType::Unknown, false, out_allocation);
}

bool IRHIRootSignatureHelper::AddSRVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation)
{
    return AddRootParameterWithRegisterCount(parameter_name, RHIRootParameterType::SRV, 1, 0, RHIRootParameterDescriptorRangeType::Unknown, false, out_allocation);
}

bool IRHIRootSignatureHelper::AddUAVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation)
{
    return AddRootParameterWithRegisterCount(parameter_name, RHIRootParameterType::UAV, 1, 0, RHIRootParameterDescriptorRangeType::Unknown, false, out_allocation);
}

bool IRHIRootSignatureHelper::AddTableRootParameter(const std::string& parameter_name,
                                                    RHIRootParameterDescriptorRangeType table_type, unsigned table_register_count, bool is_bindless, RootSignatureAllocation& out_allocation)
{
    return AddRootParameterWithRegisterCount(parameter_name, RHIRootParameterType::DescriptorTable, table_register_count, 0, table_type, is_bindless, out_allocation);
}

bool IRHIRootSignatureHelper::AddSampler(const std::string& sampler_name, RHIStaticSamplerAddressMode address_mode,
                                         RHIStaticSamplerFilterMode filter_mode, RootSignatureAllocation& out_allocation)
{
    out_allocation.parameter_index = m_layout.sampler_elements.size();
    out_allocation.register_index = m_layout.sampler_elements.size();
    m_layout.sampler_elements.push_back({sampler_name, out_allocation.parameter_index, address_mode, filter_mode});
    
    return true;
}

bool IRHIRootSignatureHelper::SetUsage(RHIRootSignatureUsage usage)
{
    m_usage = usage;
    return true;
}

bool IRHIRootSignatureHelper::BuildRootSignature(IRHIDevice& device)
{
    GLTF_CHECK(!m_root_signature);

    m_root_signature = RHIResourceFactory::CreateRHIResource<IRHIRootSignature>();
    m_root_signature->AllocateRootSignatureSpace(m_layout.ParameterCount(), m_layout.SamplerCount());
    m_root_signature->SetUsage(m_usage);
    
    for (const auto& parameter : m_layout.parameter_elements)
    {
        const auto& parameter_values = parameter.second;
        for (const auto& parameter_value : parameter_values)
        {
            switch (parameter.first) {
            case RHIRootParameterType::Constant:
                m_root_signature->GetRootParameter(parameter_value.parameter_index).InitAsConstant(parameter_value.constant_value, parameter_value.register_range.first);
                break;
            case RHIRootParameterType::CBV:
                m_root_signature->GetRootParameter(parameter_value.parameter_index).InitAsCBV(parameter_value.register_range.first);
                break;
            case RHIRootParameterType::SRV:
                m_root_signature->GetRootParameter(parameter_value.parameter_index).InitAsSRV(parameter_value.register_range.first);
                break;
            case RHIRootParameterType::UAV:
                m_root_signature->GetRootParameter(parameter_value.parameter_index).InitAsUAV(parameter_value.register_range.first);
                break;
            case RHIRootParameterType::DescriptorTable:
                {
                    RHIRootParameterDescriptorRangeDesc range_desc =
                        {
                            parameter_value.table_type,
                        parameter_value.register_range.first,
                        parameter_value.register_range.second - parameter_value.register_range.first,
                            parameter_value.is_bindless
                        };
                    m_root_signature->GetRootParameter(parameter_value.parameter_index).InitAsDescriptorTableRange(1, &range_desc);
                }
                break;
            case RHIRootParameterType::Unknown:
            default: GLTF_CHECK(false);
            }    
        }
    }

    
    for (const auto& sampler : m_layout.sampler_elements)
    {
        m_root_signature->GetStaticSampler(sampler.sample_index).InitStaticSampler(sampler.sample_index, sampler.address_mode, sampler.filter_mode);
    }
    
    return m_root_signature->InitRootSignature(device);
}

IRHIRootSignature& IRHIRootSignatureHelper::GetRootSignature() const
{
    GLTF_CHECK(m_root_signature);
    return *m_root_signature;
}

const RootSignatureLayout& IRHIRootSignatureHelper::GetRootSignatureLayout() const
{
    return m_layout;
}
