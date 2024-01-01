#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIUtils.h"

template<typename UploadStructType, unsigned count>
class glTFRenderInterface32BitConstant : public glTFRenderInterfaceWithRSAllocation
{
public:
    bool Apply32BitConstants(glTFRenderResourceManager& resource_manager, unsigned data, bool isGraphicsPipeline)
    {
        RETURN_IF_FALSE(RHIUtils::Instance().SetConstant32BitToRootParameterSlot(resource_manager.GetCommandListForRecord(),
            m_allocation.parameter_index, data, count, isGraphicsPipeline))

        return true;
    }

protected:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        return true;
    }
    
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override
    {
        // Should be apply per draw!
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddConstantRootParameter(UploadStructType::Name, 0, m_allocation);
    }
    
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        // Update light info shader define
        char registerIndexValue[32] = {'\0'};
    
        (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d, space%u)", GetRSAllocation().register_index, GetRSAllocation().space);
        out_shader_pre_define_macros.AddMacro(UploadStructType::Name, registerIndexValue);
    }
};
