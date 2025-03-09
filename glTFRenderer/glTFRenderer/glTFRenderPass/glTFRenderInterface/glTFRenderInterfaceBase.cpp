#include "glTFRenderInterfaceBase.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool glTFRenderInterfaceBase::InitInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(PreInitInterfaceImpl(resource_manager))
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->PreInitInterfaceImpl(resource_manager))
    }
    
    RETURN_IF_FALSE(InitInterfaceImpl(resource_manager))    
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->InitInterfaceImpl(resource_manager))     
    }

    RETURN_IF_FALSE(PostInitInterfaceImpl(resource_manager))
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->PostInitInterfaceImpl(resource_manager))
    }
    
    return true;
}

bool glTFRenderInterfaceBase::ApplyInterface(glTFRenderResourceManager& resource_manager, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    for (const auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->ApplyInterfaceImpl(command_list, pipeline_type,  descriptor_updater,resource_manager.GetCurrentBackBufferIndex()))
    }

    RETURN_IF_FALSE(ApplyInterfaceImpl(command_list, pipeline_type, descriptor_updater, resource_manager.GetCurrentBackBufferIndex()))

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

 bool glTFRenderInterfaceBase::PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    for (auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->PostInitInterfaceImpl(resource_manager));
    }
    
    return true;
}

bool glTFRenderInterfaceBase::ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type,
    IRHIDescriptorUpdater& descriptor_updater, unsigned frame_index)
{
    for (auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->ApplyInterfaceImpl(command_list, pipeline_type, descriptor_updater, frame_index));
    }
    
    return true;
}

 bool glTFRenderInterfaceBase::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    for (auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->InitInterfaceImpl(resource_manager));
    }
    
    return true;
}

 bool glTFRenderInterfaceBase::PreInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    for (auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->PreInitInterfaceImpl(resource_manager));
    }
    
    return true;
}

 bool glTFRenderInterfaceBase::ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature)
{
    for (auto& sub_interface : m_sub_interfaces)
    {
        RETURN_IF_FALSE(sub_interface->ApplyRootSignatureImpl(root_signature));
    }
    
    return true;
}

 void glTFRenderInterfaceBase::ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const
{
    for (auto& sub_interface : m_sub_interfaces)
    {
        sub_interface->ApplyShaderDefineImpl(out_shader_pre_define_macros);
    }
}

void glTFRenderInterfaceBase::AddInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface)
{
    m_sub_interfaces.push_back(render_interface);
}

std::shared_ptr<IRHIDescriptorTable> glTFRenderInterfaceBase::CreateDescriptorTable()
{
    return RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
}

void glTFRenderInterfaceWithRSAllocation::ApplyShaderDefineImpl(
    RHIShaderPreDefineMacros& out_shader_pre_define_macros ) const
{
    GetRSAllocation().AddShaderDefine(out_shader_pre_define_macros);
}
