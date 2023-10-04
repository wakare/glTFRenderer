#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

class glTFRenderInterfaceShaderResourceView : public glTFRenderInterfaceWithRSAllocation
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
    
    virtual bool ApplyRootSignature(IRHIRootSignatureHelper& rootSignature) override
    {
        return rootSignature.AddTableRootParameter("TableParameter", RHIRootParameterDescriptorRangeType::SRV, m_max_srv_count, m_allocation);
    }

    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override
    {
        return true;
    }
    
protected:
    unsigned m_max_srv_count;
};
