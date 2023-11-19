#include "glTFComputePassLighting.h"
#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFLight/glTFPointLight.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"

glTFComputePassLighting::glTFComputePassLighting()
    : m_base_color_RT(nullptr)
    , m_normal_RT(nullptr)
    , m_base_color_SRV(0)
    , m_depth_SRV(0)
    , m_normal_SRV(0)
    , m_output_UAV(0)
    , m_dispatch_count({0, 0, 0})
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());

    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>>();
    sampler_interface->SetSamplerRegisterIndexName("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
}

const char* glTFComputePassLighting::PassName()
{
    return "LightComputePass";
}

bool glTFComputePassLighting::InitPass(glTFRenderResourceManager& resource_manager)
{
    IRHIRenderTargetDesc lighting_output_desc;
    lighting_output_desc.width = resource_manager.GetSwapchain().GetWidth();
    lighting_output_desc.height = resource_manager.GetSwapchain().GetHeight();
    lighting_output_desc.name = "LightingOutput";
    lighting_output_desc.isUAV = true;
    lighting_output_desc.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM;
    lighting_output_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_lighting_output_RT = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, RHIDataFormat::R8G8B8A8_UNORM, lighting_output_desc);
    
    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_output_RT,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_COPY_SOURCE))
    
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))
    
    return true;
}

bool glTFComputePassLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_output_RT,
        RHIResourceStateType::STATE_COPY_SOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_base_color_RT,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_normal_RT,
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(),
        RHIResourceStateType::STATE_DEPTH_READ, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        m_base_color_and_depth_allocation.parameter_index, m_main_descriptor_heap->GetGPUHandle(0), GetPipelineType() == PipelineType::Graphics))    

    RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
        m_output_allocation.parameter_index,
        m_main_descriptor_heap->GetGPUHandle(3),
        GetPipelineType() == PipelineType::Graphics);

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer())

    return true;
}

bool glTFComputePassLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_lighting_output_RT,
        RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_COPY_SOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_base_color_RT,
        RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_normal_RT,
        RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetDepthRT(),
        RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_DEPTH_READ))

    // Copy compute result to swapchain back buffer
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_COPY_DEST))

    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, resource_manager.GetCurrentFrameSwapchainRT(), *m_lighting_output_RT))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, resource_manager.GetCurrentFrameSwapchainRT(),
            RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_RENDER_TARGET))
    
    return true;
}

DispatchCount glTFComputePassLighting::GetDispatchCount() const
{
    return m_dispatch_count;
} 

bool glTFComputePassLighting::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
                                                    const glTFSceneObjectBase& object)
{
    const glTFLightBase* light = dynamic_cast<const glTFLightBase*>(&object);
    if (!light)
    {
        return false;
    }
    
    return GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateLightInfo(*light);
}

bool glTFComputePassLighting::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::FinishProcessSceneObject(resource_manager))

    return true;
}

size_t glTFComputePassLighting::GetMainDescriptorHeapSize()
{
    return 64;
}

bool glTFComputePassLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))
    
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("BaseColorAndDepth", RHIRootParameterDescriptorRangeType::SRV, 3, false, m_base_color_and_depth_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("Output", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_output_allocation))

    return true;
}

bool glTFComputePassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    m_dispatch_count = {resource_manager.GetSwapchain().GetWidth() / 8, resource_manager.GetSwapchain().GetHeight() / 8, 1};
    
    m_base_color_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassColor");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_base_color_RT, {m_base_color_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_base_color_SRV))
    
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            resource_manager.GetDepthRT(), {RHIDataFormat::R32_FLOAT, RHIResourceDimension::TEXTURE2D}, m_depth_SRV))
    
    m_normal_RT = resource_manager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassNormal");
    RETURN_IF_FALSE(m_main_descriptor_heap->CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_normal_RT, {m_normal_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_normal_SRV))

    RETURN_IF_FALSE(m_main_descriptor_heap->CreateUnOrderAccessViewInDescriptorHeap(resource_manager.GetDevice(), m_main_descriptor_heap->GetUsedDescriptorCount(),
            *m_lighting_output_RT, {m_lighting_output_RT->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D}, m_output_UAV))
    
    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\LightingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shaderMacros = GetComputePipelineStateObject().GetShaderMacros();

    // Add albedo, normal, depth register define
    shaderMacros.AddSRVRegisterDefine("ALBEDO_TEX_REGISTER_INDEX", m_base_color_and_depth_allocation.register_index, m_base_color_and_depth_allocation.space);
    shaderMacros.AddSRVRegisterDefine("DEPTH_TEX_REGISTER_INDEX", m_base_color_and_depth_allocation.register_index + 1, m_base_color_and_depth_allocation.space);
    shaderMacros.AddSRVRegisterDefine("NORMAL_TEX_REGISTER_INDEX", m_base_color_and_depth_allocation.register_index + 2, m_base_color_and_depth_allocation.space);
    shaderMacros.AddUAVRegisterDefine("OUTPUT_TEX_REGISTER_INDEX", m_output_allocation.register_index, m_base_color_and_depth_allocation.space);

    return true;
}