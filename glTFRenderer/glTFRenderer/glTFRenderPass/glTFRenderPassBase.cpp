#include "glTFRenderPassBase.h"

#include <imgui.h>

#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassBase::~glTFRenderPassBase()
= default;

bool glTFRenderPassBase::InitPass(glTFRenderResourceManager& resource_manager)
{    
    m_main_descriptor_heap = RHIResourceFactory::CreateRHIResource<IRHIDescriptorHeap>();
    RETURN_IF_FALSE(m_main_descriptor_heap->InitDescriptorHeap(resource_manager.GetDevice(),
        {static_cast<unsigned>(GetMainDescriptorHeapSize()), RHIDescriptorHeapType::CBV_SRV_UAV, true}))

    switch (GetPipelineType())
    {
    case PipelineType::Graphics:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIGraphicsPipelineStateObject>();
        break;
    case PipelineType::Compute:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIComputePipelineStateObject>();
        break;
    case PipelineType::RayTracing:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIRayTracingPipelineStateObject>();
        break;
    default: GLTF_CHECK(false);
    }

    // Init root signature
    m_root_signature_helper.SetUsage(RHIRootSignatureUsage::Default);
    RETURN_IF_FALSE(SetupRootSignature(resource_manager))
    RETURN_IF_FALSE(m_root_signature_helper.BuildRootSignature(resource_manager.GetDevice()))
    
    RETURN_IF_FALSE(SetupPipelineStateObject(resource_manager))
    RETURN_IF_FALSE(m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(), m_root_signature_helper.GetRootSignature()))

    for (const auto& render_interface : m_render_interfaces)
    {
        RETURN_IF_FALSE(render_interface->InitInterface(resource_manager))    
    }
    
    return true;
}

bool glTFRenderPassBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    resource_manager.SetCurrentPSO(m_pipeline_state_object);
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorHeapArray(command_list, m_main_descriptor_heap.get(), 1))
    RETURN_IF_FALSE(RHIUtils::Instance().SetRootSignature(command_list, m_root_signature_helper.GetRootSignature(), GetPipelineType() == PipelineType::Graphics))
    
    for (const auto& render_interface : m_render_interfaces)
    {
        RETURN_IF_FALSE(render_interface->ApplyInterface(resource_manager, GetPipelineType() == PipelineType::Graphics))    
    }
    
    return true;
}

bool glTFRenderPassBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderPassBase::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool glTFRenderPassBase::UpdateGUIWidgets()
{
    ImGui::Text("%s", PassName());
    return true;
}

bool glTFRenderPassBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    for (const auto& render_interface : m_render_interfaces)
    {
        RETURN_IF_FALSE(render_interface->ApplyRootSignature(m_root_signature_helper))    
    }
    
    return true;
}

bool glTFRenderPassBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    auto& shaderMacros = m_pipeline_state_object->GetShaderMacros();
    for (const auto& render_interface : m_render_interfaces)
    {
        render_interface->ApplyShaderDefine(shaderMacros);    
    }
    
    return true;
}

void glTFRenderPassBase::AddRenderInterface(const std::shared_ptr<glTFRenderInterfaceBase>& render_interface)
{
    m_render_interfaces.push_back(render_interface);
}
