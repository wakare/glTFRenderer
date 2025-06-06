#pragma once
#include "glTFRenderInterfaceBase.h"

template<RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode>
class glTFRenderInterfaceSampler : public glTFRenderInterfaceWithRSAllocation
{
public:
    glTFRenderInterfaceSampler(const char* name)
        : glTFRenderInterfaceWithRSAllocation(name)
    {}
    
protected:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        return true;
    }

    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override
    {
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddSampler(m_name, address_mode, filter_mode, m_allocation);
    }
};
