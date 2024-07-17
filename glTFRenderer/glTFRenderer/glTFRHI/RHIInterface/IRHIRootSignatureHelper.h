#pragma once

#include "IRHIRootSignature.h"

class IRHIRootSignatureHelper
{
public:
    IRHIRootSignatureHelper(unsigned register_space = 0);

    // Only copy layout
    IRHIRootSignatureHelper(const IRHIRootSignatureHelper& other) = delete;
    IRHIRootSignatureHelper& operator=(const IRHIRootSignatureHelper& other) = delete;

    IRHIRootSignatureHelper(IRHIRootSignatureHelper&& other) = delete;
    IRHIRootSignatureHelper& operator=(IRHIRootSignatureHelper&& other) = delete;
    
    bool AddConstantRootParameter(const std::string& parameter_name, unsigned constant_value_count, RootSignatureAllocation& out_allocation);
    bool AddCBVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddSRVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddUAVRootParameter(const std::string& parameter_name, RootSignatureAllocation& out_allocation);
    bool AddTableRootParameter(const std::string& parameter_name, RHIRootParameterDescriptorRangeType table_type, unsigned table_register_count, bool
                               is_bindless, RootSignatureAllocation& out_allocation);
    bool AddSampler(const std::string& sampler_name, RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode, RootSignatureAllocation& out_allocation);

    bool SetUsage(RHIRootSignatureUsage usage);
    void SetRegisterSpace(unsigned space);
    unsigned GetRegisterSpace() const;
    bool BuildRootSignature(IRHIDevice& device, glTFRenderResourceManager& resource_manager);
    
    IRHIRootSignature& GetRootSignature() const;
    std::shared_ptr<IRHIRootSignature> GetRootSignaturePointer() const { return m_root_signature; }
    
    const RootSignatureLayout& GetRootSignatureLayout() const;
    
protected:
    bool AddRootParameterWithRegisterCount(const std::string& parameter_name, RHIRootParameterType type, unsigned register_count, unsigned constant_value, RHIRootParameterDescriptorRangeType table_type, bool
                                           is_bindless, RootSignatureAllocation& out_allocation);
    RHIRootSignatureUsage m_usage;
    RootSignatureLayout m_layout;
    std::shared_ptr<IRHIRootSignature> m_root_signature;
    bool m_contains_bindless_parameter;

    unsigned m_register_space;
};
