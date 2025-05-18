#include "IRHIRootSignatureHelper.h"

#include "RHIResourceFactoryImpl.hpp"
#include "RendererCommon.h"

void IRHIRootSignatureHelper::IncrementRegisterSpaceIndex()
{
    ++m_current_space;
}

bool IRHIRootSignatureHelper::AddRootParameterWithRegisterCount(const RootParameterInfo& parameter_info, RootSignatureAllocation& out_allocation)
{
    RHIShaderRegisterType register_type = RHIShaderRegisterType::Unknown;
    switch (parameter_info.type) {
    case RHIRootParameterType::Constant:
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
            switch (parameter_info.table_parameter_info.table_type) {
            case RHIDescriptorRangeType::CBV:
                register_type = RHIShaderRegisterType::b;
                break;
            case RHIDescriptorRangeType::SRV:
                register_type = RHIShaderRegisterType::t;
                break;
            case RHIDescriptorRangeType::UAV:
                register_type = RHIShaderRegisterType::u;
                break;
            case RHIDescriptorRangeType::Unknown:
                GLTF_CHECK(false);
            default: ;
            }
        }
        break;
    case RHIRootParameterType::Sampler:
        register_type = RHIShaderRegisterType::s;
        break;
    case RHIRootParameterType::Unknown:
    default: GLTF_CHECK(false);
    }
    
    out_allocation.parameter_name = parameter_info.parameter_name;
    out_allocation.type = parameter_info.type;
    out_allocation.register_type = register_type;
    
    if (parameter_info.type == RHIRootParameterType::Sampler)
    {
        out_allocation.global_parameter_index = m_layout.sampler_elements.size();
        out_allocation.register_begin_index = m_layout.sampler_elements.size();

        if (m_sampler_space < 0)
        {
            if (m_current_space == m_normal_resource_space ||
                m_bindless_spaces.contains(m_current_space))
            {
                IncrementRegisterSpaceIndex();
            }
            m_sampler_space = m_current_space;
        }
        out_allocation.space = m_sampler_space;
        out_allocation.local_space_parameter_index = m_space_available_attribute_index[(out_allocation.space)]; 
        m_space_available_attribute_index[(out_allocation.space)]++;
        
        RootSignatureStaticSamplerElement sampler_element{};
        sampler_element.sampler_name = parameter_info.parameter_name;
        sampler_element.register_space = out_allocation.space;
        sampler_element.sample_index = out_allocation.local_space_parameter_index;
        sampler_element.address_mode = parameter_info.sampler_parameter_info.address_mode;
        sampler_element.filter_mode = parameter_info.sampler_parameter_info.filter_mode; 
        m_layout.sampler_elements.push_back(sampler_element);
    }
    else
    {
        RootSignatureParameterElement element{};
        element.name = parameter_info.parameter_name;
        element.global_parameter_index = m_layout.last_parameter_index++;
        element.is_buffer = parameter_info.is_buffer;
        
        if (parameter_info.type == RHIRootParameterType::Constant)
        {
            element.constant_value_count = parameter_info.constant_parameter_info.constant_value;
        }
        else if (parameter_info.type == RHIRootParameterType::DescriptorTable)
        {
            element.table_type = parameter_info.table_parameter_info.table_type;
            element.is_bindless = parameter_info.table_parameter_info.is_bindless;
        }
        
        if (element.is_bindless)
        {
            unsigned register_start = 0;
            unsigned register_end = register_start + parameter_info.register_count;
            element.register_range = {register_start, register_end};
            if (m_current_space == m_sampler_space ||
                m_current_space == m_normal_resource_space ||
                m_bindless_spaces.contains(m_current_space))
            {
                IncrementRegisterSpaceIndex();
            }
            element.space = m_current_space;
            m_bindless_spaces.emplace(element.space);
        }
        else
        {
            unsigned register_start = m_layout.last_register_index[register_type];
            unsigned register_end = register_start + parameter_info.register_count;
            element.register_range = {register_start, register_end};
            m_layout.last_register_index[register_type] += parameter_info.register_count;

            if (m_normal_resource_space < 0)
            {
                if (m_current_space == m_sampler_space || m_bindless_spaces.contains(m_current_space))
                {
                    IncrementRegisterSpaceIndex();
                }
                m_normal_resource_space = m_current_space;
            }
            element.space = m_normal_resource_space;
        }
        
        out_allocation.register_begin_index = element.register_range.first;
        out_allocation.register_end_index = element.register_range.second;
        out_allocation.global_parameter_index = element.global_parameter_index;
        out_allocation.space = element.space;
        out_allocation.bindless_descriptor = element.is_bindless;
        out_allocation.local_space_parameter_index = m_space_available_attribute_index[(out_allocation.space)];
        m_space_available_attribute_index[(out_allocation.space)]++;
        
        element.local_space_parameter_index = out_allocation.local_space_parameter_index;
        m_layout.parameter_elements[parameter_info.type].push_back(element);
    }
    
    return true;
}

IRHIRootSignatureHelper::IRHIRootSignatureHelper(int register_space)
    : m_usage(RHIRootSignatureUsage::None)
    , m_contains_bindless_parameter(false)
    , m_current_space(register_space)
    , m_normal_resource_space(-1)
    , m_sampler_space(-1)
{
}

bool IRHIRootSignatureHelper::AddConstantRootParameter(const std::string& parameter_name, unsigned constant_value_count, RootSignatureAllocation& out_allocation)
{
    RootParameterInfo info
    {
        .parameter_name = parameter_name,
        .type = RHIRootParameterType::Constant,
        .register_count = 1,
        .constant_parameter_info =
        {
            .constant_value = constant_value_count
        }
    };
    return AddRootParameterWithRegisterCount(info, out_allocation);
}

bool IRHIRootSignatureHelper::AddCBVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation)
{
    RootParameterInfo info
    {
        .parameter_name = parameter_name,
        .type = RHIRootParameterType::CBV,
        .register_count = 1,
    };
    return AddRootParameterWithRegisterCount(info, out_allocation);
}

bool IRHIRootSignatureHelper::AddSRVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation)
{
    RootParameterInfo info
    {
        .parameter_name = parameter_name,
        .type = RHIRootParameterType::SRV,
        .register_count = 1,
    };
    return AddRootParameterWithRegisterCount(info, out_allocation);
}

bool IRHIRootSignatureHelper::AddUAVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation)
{
    RootParameterInfo info
    {
        .parameter_name = parameter_name,
        .type = RHIRootParameterType::UAV,
        .register_count = 1,
    };
    return AddRootParameterWithRegisterCount(info, out_allocation);
}

bool IRHIRootSignatureHelper::AddTableRootParameter(const std::string& parameter_name,
                                                    const TableRootParameterDesc& desc, RootSignatureAllocation& out_allocation)
{
    RootParameterInfo info
    {
        .parameter_name = parameter_name,
        .type = RHIRootParameterType::DescriptorTable,
        .register_count = desc.table_register_count,
        .is_buffer = desc.is_buffer,
        .table_parameter_info =
        {
            .table_type = desc.table_type,
            .is_bindless = desc.is_bindless,
        }
    };
    
    return AddRootParameterWithRegisterCount(info, out_allocation);
}

bool IRHIRootSignatureHelper::AddSampler(const std::string& sampler_name, RHIStaticSamplerAddressMode address_mode,
                                         RHIStaticSamplerFilterMode filter_mode, RootSignatureAllocation& out_allocation)
{
    RootParameterInfo info
    {
        .parameter_name = sampler_name,
        .type = RHIRootParameterType::Sampler,
        .register_count = 1,
        .sampler_parameter_info =
        {
            .address_mode = address_mode,
            .filter_mode = filter_mode,
        }
    };
    
    return AddRootParameterWithRegisterCount(info, out_allocation);
}

bool IRHIRootSignatureHelper::SetUsage(RHIRootSignatureUsage usage)
{
    m_usage = usage;
    return true;
}

bool IRHIRootSignatureHelper::BuildRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager)
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
                m_root_signature->GetRootParameter(parameter_value.global_parameter_index).InitAsConstant(parameter_value.constant_value_count, parameter_value.register_range.first, parameter_value.space);
                break;
            case RHIRootParameterType::CBV:
                m_root_signature->GetRootParameter(parameter_value.global_parameter_index).InitAsCBV(parameter_value.local_space_parameter_index, parameter_value.register_range.first, parameter_value.space);
                break;
            case RHIRootParameterType::SRV:
                m_root_signature->GetRootParameter(parameter_value.global_parameter_index).InitAsSRV(parameter_value.local_space_parameter_index, parameter_value.register_range.first, parameter_value.space);
                break;
            case RHIRootParameterType::UAV:
                m_root_signature->GetRootParameter(parameter_value.global_parameter_index).InitAsUAV(parameter_value.local_space_parameter_index, parameter_value.register_range.first, parameter_value.space);
                break;
            case RHIRootParameterType::DescriptorTable:
                {
                    RHIDescriptorRangeDesc range_desc =
                        {
                            parameter_value.table_type,
                            parameter_value.register_range.first,
                            parameter_value.space,
                            parameter_value.register_range.second - parameter_value.register_range.first,
                            parameter_value.is_bindless,
                            parameter_value.is_buffer
                        };
                    m_root_signature->GetRootParameter(parameter_value.global_parameter_index).InitAsDescriptorTableRange(parameter_value.local_space_parameter_index,  1, &range_desc);
                }
                break;
            case RHIRootParameterType::Unknown:
            default: GLTF_CHECK(false);
            }    
        }
    }

    for (const auto& sampler : m_layout.sampler_elements)
    {
        m_root_signature->GetStaticSampler(sampler.sample_index).InitStaticSampler(device, sampler.register_space, sampler.sample_index, sampler.address_mode, sampler.filter_mode);
    }
    
    return m_root_signature->InitRootSignature(device, descriptor_manager);
}

IRHIRootSignature& IRHIRootSignatureHelper::GetRootSignature() const
{
    GLTF_CHECK(m_root_signature);
    return *m_root_signature;
}

std::shared_ptr<IRHIRootSignature> IRHIRootSignatureHelper::GetRootSignaturePointer() const
{
    return m_root_signature;
}

const RootSignatureLayout& IRHIRootSignatureHelper::GetRootSignatureLayout() const
{
    return m_layout;
}
