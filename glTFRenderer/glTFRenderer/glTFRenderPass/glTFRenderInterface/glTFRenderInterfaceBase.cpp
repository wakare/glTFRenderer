#include "glTFRenderInterfaceBase.h"

bool glTFRenderInterfaceBase::InitInterface(glTFRenderResourceManager& resource_manager)
{
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->InitInterfaceImpl(resource_manager))     
    }

    RETURN_IF_FALSE(InitInterfaceImpl(resource_manager))
     
    return true;
}

bool glTFRenderInterfaceBase::ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline)
{
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->ApplyInterfaceImpl(resource_manager, isGraphicsPipeline))     
    }

    RETURN_IF_FALSE(ApplyInterfaceImpl(resource_manager, isGraphicsPipeline))
    
    return true;
}

bool glTFRenderInterfaceBase::ApplyRootSignature(IRHIRootSignatureHelper& root_signature)
{
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->ApplyRootSignatureImpl(root_signature))     
    }

    RETURN_IF_FALSE(ApplyRootSignatureImpl(root_signature))     
     
    return true;
}

void glTFRenderInterfaceBase::ApplyShaderDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const
{
    for (const auto& sub_interface : m_sub_interfaces)
    {
        sub_interface->ApplyShaderDefineImpl(out_shader_pre_define_macros);
    }

    ApplyShaderDefineImpl(out_shader_pre_define_macros);
}

void glTFRenderInterfaceBase::AddInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface)
{
    m_sub_interfaces.push_back(render_interface);
}
