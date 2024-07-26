#pragma once
#include "glTFRenderInterfaceBase.h"

template<RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode>
class glTFRenderInterfaceSampler : public glTFRenderInterfaceWithRSAllocation
{
public:
    void SetSamplerRegisterIndexName(const std::string& name)
    {
        m_register_index_name = name;
    }
    
protected:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        return true;
    }

    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater) override
    {
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddSampler(m_register_index_name, address_mode, filter_mode, m_allocation);
    }
    
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        AddRootSignatureShaderRegisterDefine(out_shader_pre_define_macros, m_register_index_name);
    }

protected:
    std::string m_register_index_name;
};
