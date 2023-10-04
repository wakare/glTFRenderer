#pragma once
#include <map>

#include "IRHIRootSignature.h"

struct RootSignatureAllocation
{
    RootSignatureAllocation()
        : parameter_index(0)
        , register_index(0)
    {
    }

    unsigned parameter_index;
    unsigned register_index;
};

struct RootSignatureStaticSamplerElement
{
    std::string sampler_name;
    unsigned sample_index;
    RHIStaticSamplerAddressMode address_mode;
    RHIStaticSamplerFilterMode filter_mode;
};

struct RootSignatureParameterElement
{
    std::string name;
    unsigned parameter_index;
    std::pair<unsigned, unsigned> register_range;
    unsigned constant_value;
    RHIRootParameterDescriptorRangeType table_type;
    bool is_bindless;
};

struct RootSignatureLayout
{
    RootSignatureLayout()
        : last_parameter_index(0)
    {
    }

    std::map<RHIRootParameterType, std::vector<RootSignatureParameterElement>> parameter_elements;
    std::vector<RootSignatureStaticSamplerElement> sampler_elements;

    std::map<RHIShaderRegisterType, unsigned> last_register_index;
    unsigned last_parameter_index;
    
    unsigned ParameterCount() const { return last_parameter_index; }
    unsigned SamplerCount() const {return sampler_elements.size(); }
};

class IRHIRootSignatureHelper
{
public:
    IRHIRootSignatureHelper();
    
    bool AddConstantRootParameter(const std::string& parameter_name, unsigned constant_value, RootSignatureAllocation& out_allocation);
    bool AddCBVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddSRVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddUAVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddTableRootParameter(const std::string& parameter_name, RHIRootParameterDescriptorRangeType table_type, unsigned table_register_count, bool
                               is_bindless, RootSignatureAllocation& out_allocation);
    bool AddSampler(const std::string& sampler_name, RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode, RootSignatureAllocation& out_allocation);

    bool SetUsage(RHIRootSignatureUsage usage);
    bool BuildRootSignature(IRHIDevice& device);

    IRHIRootSignature& GetRootSignature() const;
    const RootSignatureLayout& GetRootSignatureLayout() const;
    
protected:
    bool AddRootParameterWithRegisterCount(const std::string& parameter_name, RHIRootParameterType type, unsigned register_count, unsigned constant_value, RHIRootParameterDescriptorRangeType table_type, bool
                                           is_bindless, RootSignatureAllocation& out_allocation);
    RHIRootSignatureUsage m_usage;
    RootSignatureLayout m_layout;
    std::shared_ptr<IRHIRootSignature> m_root_signature;
};
