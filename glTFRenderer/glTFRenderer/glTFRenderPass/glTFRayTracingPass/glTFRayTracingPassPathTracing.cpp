#include "glTFRayTracingPassPathTracing.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
//#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceRadiosityScene.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSingleConstantBuffer.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIUtils.h"

struct RHIShaderBindingTableRecordPathTracing : RHIShaderTableRecordBase
{
    RHIShaderBindingTableRecordPathTracing(unsigned material_id)
        : m_material_id(material_id)
    {
    }

    virtual void* GetData() override
    {
        return &m_material_id;   
    }
    virtual size_t GetSize() override
    {
        return 32;
    }

    unsigned m_material_id;
};

glTFRayTracingPassPathTracing::glTFRayTracingPassPathTracing()
{
}

const char* glTFRayTracingPassPathTracing::PassName()
{
    return "PathTracingPass";
}

bool glTFRayTracingPassPathTracing::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<RayTracingPathTracingPassOptions>>());
    
    return true;
}

bool glTFRayTracingPassPathTracing::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitPass(resource_manager))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PreRenderPass(resource_manager))
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    GetResourceTexture(RenderPassResourceTableId::RayTracingSceneOutput)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    GetResourceTexture(RenderPassResourceTableId::ScreenUVOffset)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);

    BindDescriptor(command_list, m_output_allocation, *GetResourceDescriptor(RenderPassResourceTableId::RayTracingSceneOutput));
    BindDescriptor(command_list, m_screen_uv_offset_allocation, *GetResourceDescriptor(RenderPassResourceTableId::ScreenUVOffset));
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<RayTracingPathTracingPassOptions>>()->UploadBuffer(resource_manager, &m_pass_options, 0, sizeof(m_pass_options)))
    
    return true;
}

bool glTFRayTracingPassPathTracing::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::PostRenderPass(resource_manager))

    return true;
}

bool glTFRayTracingPassPathTracing::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    // Non-bindless table parameter should be added first
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("OUTPUT_REGISTER_INDEX", {RHIDescriptorRangeType::UAV, 1, false, false}, m_output_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("SCREEN_UV_OFFSET_REGISTER_INDEX", {RHIDescriptorRangeType::UAV, 1, false, false}, m_screen_uv_offset_allocation))
    
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFRayTracingPassPathTracing::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::SetupPipelineStateObject(resource_manager))
    
    BindShaderCode("glTFResources/ShaderSource/RayTracing/PathTracingMain.hlsl",
                                                      RHIShaderType::RayTracing, "");
    
    auto& shader_macros = GetShaderMacros();
    m_output_allocation.AddShaderDefine(shader_macros);
    m_screen_uv_offset_allocation.AddShaderDefine(shader_macros);
    
    // One ray type mapping one hit group desc
    GetRayTracingPipelineStateObject().AddHitGroupDesc({GetPrimaryRayHitGroupName(),
        GetPrimaryRayCHFunctionName(),
        "",
        ""
    });
    GetRayTracingPipelineStateObject().AddHitGroupDesc({GetShadowRayHitGroupName(),
        "",
        "",
        ""
    });

    IRHIRayTracingPipelineStateObject::RHIRayTracingConfig config;
    config.payload_size = sizeof(float) * 12;
    config.attribute_size = sizeof(float) * 2;
    config.max_recursion_count = 1;
    GetRayTracingPipelineStateObject().SetConfig(config);

    GetRayTracingPipelineStateObject().SetExportFunctionNames(
        {
            GetRayGenFunctionName(),
            GetPrimaryRayCHFunctionName(),
            GetPrimaryRayMissFunctionName(),
            GetShadowRayMissFunctionName(),
        });
    
    return true;
}

const char* glTFRayTracingPassPathTracing::GetRayGenFunctionName()
{
    return "PathTracingRayGen";
}

const char* glTFRayTracingPassPathTracing::GetPrimaryRayCHFunctionName()
{
    return "PrimaryRayClosestHit";
}

const char* glTFRayTracingPassPathTracing::GetPrimaryRayMissFunctionName()
{
    return "PrimaryRayMiss";
}

const char* glTFRayTracingPassPathTracing::GetPrimaryRayHitGroupName()
{
    return "MyHitGroup";
}

const char* glTFRayTracingPassPathTracing::GetShadowRayMissFunctionName()
{
    return "ShadowRayMiss";
}

const char* glTFRayTracingPassPathTracing::GetShadowRayHitGroupName()
{
    return "ShadowHitGroup";
}

bool glTFRayTracingPassPathTracing::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::UpdateGUIWidgets())

    ImGui::SliderInt("MaxBounceCount", &m_pass_options.max_bounce_count, 1, 64);
    ImGui::SliderInt("CandidateLightCount", &m_pass_options.candidate_light_count, 4, 32);
    ImGui::SliderInt("SamplesPerPixel", &m_pass_options.samples_per_pixel, 1, 64);
    ImGui::Checkbox("CheckVisibilityForAllCandidates", (bool*)&m_pass_options.check_visibility_for_all_candidates);
    ImGui::Checkbox("RISLightSampling", (bool*)&m_pass_options.ris_light_sampling);
    ImGui::Checkbox("DebugRadiosity", (bool*)&m_pass_options.debug_radiosity);
    
    return true;
}

bool glTFRayTracingPassPathTracing::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRayTracingPassWithMesh::InitResourceTable(resource_manager))
    
    const unsigned width = resource_manager.GetSwapChain().GetWidth();
    const unsigned height = resource_manager.GetSwapChain().GetHeight();
    
    auto output_desc = RHITextureDesc::MakeRayTracingSceneOutputTextureDesc(width, height);
    AddExportTextureResource(RenderPassResourceTableId::RayTracingSceneOutput, RHITextureDesc::MakeRayTracingSceneOutputTextureDesc(width, height), 
    {
                    output_desc.GetDataFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                    });
    auto uv_offset_desc = RHITextureDesc::MakeScreenUVOffsetTextureDesc(width, height);
    AddExportTextureResource(RenderPassResourceTableId::ScreenUVOffset, uv_offset_desc, 
    {
                    uv_offset_desc.GetDataFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                    });
    
    return true;
}
