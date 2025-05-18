#pragma once

#include <set>

#include "IRHIRootSignature.h"

class RHICORE_API IRHIRootSignatureHelper
{
public:
    IRHIRootSignatureHelper(int register_space = 0);

    // Only copy layout
    IRHIRootSignatureHelper(const IRHIRootSignatureHelper& other) = delete;
    IRHIRootSignatureHelper& operator=(const IRHIRootSignatureHelper& other) = delete;

    IRHIRootSignatureHelper(IRHIRootSignatureHelper&& other) = delete;
    IRHIRootSignatureHelper& operator=(IRHIRootSignatureHelper&& other) = delete;
    
    bool AddConstantRootParameter(const std::string& parameter_name, unsigned constant_value_count, RootSignatureAllocation& out_allocation);
    bool AddCBVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddSRVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddUAVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);

    struct TableRootParameterDesc
    {
        RHIDescriptorRangeType table_type;
        unsigned table_register_count;
        bool is_buffer;
        bool is_bindless;
    };
    
    bool AddTableRootParameter(const std::string& parameter_name, const TableRootParameterDesc& desc, RootSignatureAllocation& out_allocation);
    bool AddSampler(const std::string& sampler_name, RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode, RootSignatureAllocation& out_allocation);

    bool SetUsage(RHIRootSignatureUsage usage);
    bool BuildRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager);
    
    IRHIRootSignature& GetRootSignature() const;
    std::shared_ptr<IRHIRootSignature> GetRootSignaturePointer() const;
    
    const RootSignatureLayout& GetRootSignatureLayout() const;
    
protected:
    struct RootParameterInfo
    {
        std::string parameter_name;
        RHIRootParameterType type;
        unsigned register_count;
        bool is_buffer;
        
        union
        {
            struct ConstantParameterInfo
            {
                unsigned constant_value;
            } constant_parameter_info;

            struct TableParameterInfo
            {
                RHIDescriptorRangeType table_type;
                bool is_bindless;
            } table_parameter_info;

            struct SamplerParameterInfo
            {
                RHIStaticSamplerAddressMode address_mode;
                RHIStaticSamplerFilterMode filter_mode;
            } sampler_parameter_info;
        };
    };
    void IncrementRegisterSpaceIndex();
    
    bool AddRootParameterWithRegisterCount( const RootParameterInfo& parameter_info, RootSignatureAllocation& out_allocation);
    
    RHIRootSignatureUsage m_usage;
    RootSignatureLayout m_layout;
    std::shared_ptr<IRHIRootSignature> m_root_signature;
    bool m_contains_bindless_parameter;

    int m_current_space;
    int m_normal_resource_space;
    int m_sampler_space;
    std::set<int> m_bindless_spaces;

    std::map<unsigned, unsigned> m_space_available_attribute_index;
};
