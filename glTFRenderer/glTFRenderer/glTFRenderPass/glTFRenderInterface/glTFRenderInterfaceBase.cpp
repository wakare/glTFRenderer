#include "glTFRenderInterfaceBase.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool glTFRenderInterfaceBase::InitInterface(glTFRenderResourceManager& resource_manager)
{
    TraverseWithLambda(*this, [&](glTFRenderInterfaceBase& render_interface)
    {
        render_interface.PreInitInterfaceImpl(resource_manager);
    });
    TraverseWithLambda(*this, [&](glTFRenderInterfaceBase& render_interface)
    {
        render_interface.InitInterfaceImpl(resource_manager);
    });
    TraverseWithLambda(*this, [&](glTFRenderInterfaceBase& render_interface)
    {
        render_interface.PostInitInterfaceImpl(resource_manager);
    });
    
    return true;
}

bool glTFRenderInterfaceBase::ApplyInterface(glTFRenderResourceManager& resource_manager, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    TraverseWithLambda(*this, [&](glTFRenderInterfaceBase& render_interface)
    {
        render_interface.ApplyInterfaceImpl(command_list, pipeline_type,  descriptor_updater,resource_manager.GetCurrentBackBufferIndex());
    });
    
    return true;
}

bool glTFRenderInterfaceBase::ApplyRootSignature(IRHIRootSignatureHelper& root_signature)
{
    TraverseWithLambda(*this, [&](glTFRenderInterfaceBase& render_interface)
    {
        render_interface.ApplyRootSignatureImpl(root_signature);
    });
    
    return true;
}

void glTFRenderInterfaceBase::ApplyShaderDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros)
{
    TraverseWithLambda(*this, [&](glTFRenderInterfaceBase& render_interface)
    {
        render_interface.ApplyShaderDefineImpl(out_shader_pre_define_macros);
    });
}

bool glTFRenderInterfaceBase::PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderInterfaceBase::ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type,
    IRHIDescriptorUpdater& descriptor_updater, unsigned frame_index)
{
    return true;
}

 bool glTFRenderInterfaceBase::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderInterfaceBase::PreInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderInterfaceBase::ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature)
{
    return true;
}

void glTFRenderInterfaceBase::ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros)
{
}

void glTFRenderInterfaceBase::AddInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface)
{
    m_sub_interfaces.push_back(render_interface);
}

std::shared_ptr<IRHIDescriptorTable> glTFRenderInterfaceBase::CreateDescriptorTable()
{
    return RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
}

void glTFRenderInterfaceBase::TraverseWithLambda(glTFRenderInterfaceBase& render_interface, const std::function<void(glTFRenderInterfaceBase&)>& lambda_function)
{
    lambda_function(render_interface);

    for (auto& sub_interface : render_interface.m_sub_interfaces)
    {
        TraverseWithLambda(*sub_interface, lambda_function);
    }
}

void glTFRenderInterfaceWithRSAllocation::ApplyShaderDefineImpl(
    RHIShaderPreDefineMacros& out_shader_pre_define_macros )
{
    GetRSAllocation().AddShaderDefine(out_shader_pre_define_macros);
}
