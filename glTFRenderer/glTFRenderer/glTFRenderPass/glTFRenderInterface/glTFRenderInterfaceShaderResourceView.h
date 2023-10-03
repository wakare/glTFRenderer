#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

class glTFRenderInterfaceShaderResourceView : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceShaderResourceView()
        : m_max_srv_count(4)
    {
        
    }

    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override
    {
        return true;
    }
    
    virtual bool SetupRootSignature(IRHIRootSignatureHelper& rootSignature)
    {
        return rootSignature.AddTableRootParameter("TableParameter", RHIRootParameterDescriptorRangeType::SRV, m_max_srv_count, m_allocation);
    }
    
protected:
    RootSignatureAllocation m_allocation;
    unsigned m_max_srv_count;
};
