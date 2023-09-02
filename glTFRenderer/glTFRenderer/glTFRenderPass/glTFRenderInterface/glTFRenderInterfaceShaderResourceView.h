#pragma once
#include "glTFRenderInterfaceBase.h"

class glTFRenderInterfaceShaderResourceView : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceShaderResourceView(
        unsigned root_parameter_srv_index,
        unsigned srv_register_index,
        unsigned max_srv_count)
        : m_root_parameter_srv_index(root_parameter_srv_index)
        , m_srv_register_index(srv_register_index)
        , m_max_srv_count(max_srv_count)
    {
        
    }

    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override
    {
        return true;
    }
    
    virtual bool SetupRootSignature(IRHIRootSignature& rootSignature) const
    {
        const RHIRootParameterDescriptorRangeDesc srv_range_desc {RHIRootParameterDescriptorRangeType::SRV, m_srv_register_index, m_max_srv_count};
        RETURN_IF_FALSE(rootSignature.GetRootParameter(m_root_parameter_srv_index).InitAsDescriptorTableRange(1, &srv_range_desc))

        return true;
    }

    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const = 0;
    
    
protected:
    unsigned m_root_parameter_srv_index;
    unsigned m_srv_register_index;
    unsigned m_max_srv_count;
};
