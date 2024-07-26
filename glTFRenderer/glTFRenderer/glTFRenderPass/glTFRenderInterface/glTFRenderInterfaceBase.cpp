#include "glTFRenderInterfaceBase.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"

bool glTFRenderInterfaceBase::InitInterface(glTFRenderResourceManager& resource_manager)
{
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->InitInterfaceImpl(resource_manager))     
    }

    RETURN_IF_FALSE(InitInterfaceImpl(resource_manager))
     
    return true;
}

bool glTFRenderInterfaceBase::ApplyInterface(glTFRenderResourceManager& resource_manager, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->ApplyInterfaceImpl(command_list, pipeline_type, descriptor_updater))     
    }

    RETURN_IF_FALSE(ApplyInterfaceImpl(command_list, pipeline_type, descriptor_updater))
    
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

void glTFRenderInterfaceWithRSAllocation::AddRootSignatureShaderRegisterDefine(
    RHIShaderPreDefineMacros& out_shader_pre_define_macros, const std::string& name, unsigned register_offset ) const
{
    char registerIndexValue[64] = {'\0'};

    std::string register_name;
    switch (GetRSAllocation().register_type) {
    case RHIShaderRegisterType::b:
        register_name = "b";
        break;
    case RHIShaderRegisterType::t:
        register_name = "t";
        break;
    case RHIShaderRegisterType::u:
        register_name = "u";
        break;
    case RHIShaderRegisterType::s:
        register_name = "s";
        break;
    case RHIShaderRegisterType::Unknown:
        GLTF_CHECK(false);
        break;
    }
    
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(%s%d, space%u)", register_name.c_str(), GetRSAllocation().register_index + register_offset, GetRSAllocation().space);
    out_shader_pre_define_macros.AddMacro(name, registerIndexValue);
}
