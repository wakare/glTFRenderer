#include "glTFRenderPassMeshOpaque.h"

#include "glTFRenderMaterialManager.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"

bool glTFRenderPassMeshOpaque::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::InitPass(resourceManager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::InitInterface(resourceManager))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneMesh::InitInterface(resourceManager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))
    RETURN_IF_FALSE(resourceManager.GetCurrentFrameFence().SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))

    LOG_FORMAT("[DEBUG] Init MeshPassOpaque finished!\n")
    return true;
}

bool glTFRenderPassMeshOpaque::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::RenderPass(resourceManager))
    
    return true;
}

bool glTFRenderPassMeshOpaque::ProcessMaterial(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material)
{
    RETURN_IF_FALSE(material.GetMaterialType() == MaterialType::Opaque)
	RETURN_IF_FALSE(resource_manager.GetMaterialManager().InitMaterialRenderResource(resource_manager, *m_main_descriptor_heap, material))
    
    return true;
}

size_t glTFRenderPassMeshOpaque::GetMainDescriptorHeapSize()
{
    return 1;
}

bool glTFRenderPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    // Init root signature
    constexpr size_t rootSignatureParameterCount = MeshOpaquePass_RootParameter_LastIndex;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    RETURN_IF_FALSE(m_root_signature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount))
    
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupRootSignature(resourceManager))
    
    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 1};
    m_root_signature->GetRootParameter(MeshOpaquePass_RootParameter_MeshMaterialTexSRV).InitAsDescriptorTableRange(1, &SRVRangeDesc);
    
    m_root_signature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);
    RETURN_IF_FALSE(m_root_signature->InitRootSignature(resourceManager.GetDevice()))

    return true;
}

bool glTFRenderPassMeshOpaque::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupPipelineStateObject(resourceManager))
    
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");

    RETURN_IF_FALSE(m_pipeline_state_object->BindInputLayout(GetVertexInputLayout()))
    
    RETURN_IF_FALSE (m_pipeline_state_object->InitPipelineStateObject(resourceManager.GetDevice(), *m_root_signature, resourceManager.GetSwapchain()))

    return true;
}

bool glTFRenderPassMeshOpaque::BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID)
{
    // Using texture SRV slot when mesh material is texture
    glTFUniqueID material_ID = m_meshes[meshID].materialID;
    if (material_ID == glTFUniqueIDInvalid)
    {
        return true;
    }
    
	return resourceManager.ApplyMaterial(material_ID, MeshOpaquePass_RootParameter_MeshMaterialTexSRV);
}

std::vector<RHIPipelineInputLayout> glTFRenderPassMeshOpaque::GetVertexInputLayout()
{
    std::vector<RHIPipelineInputLayout> inputLayouts;
    inputLayouts.push_back({g_inputLayoutNamePOSITION, 0, RHIDataFormat::R32G32B32_FLOAT, 0});
    inputLayouts.push_back({g_inputLayoutNameNORMAL, 0, RHIDataFormat::R32G32B32_FLOAT, 12});
    return inputLayouts;
}

std::vector<RHIPipelineInputLayout> glTFRenderPassMeshOpaque::ResolveVertexInputLayout(
    const glTFScenePrimitive& primitive)
{
    std::vector<RHIPipelineInputLayout> inputLayouts;
    const auto& boxVertexLayout = primitive.GetVertexLayout();

    unsigned vertexLayoutOffset = 0;
    for (const auto& vertexLayout : boxVertexLayout.elements)
    {
        switch (vertexLayout.type)
        {
        case VertexLayoutType::POSITION:
            {
                inputLayouts.push_back({g_inputLayoutNamePOSITION, 0, RHIDataFormat::R32G32B32_FLOAT, vertexLayoutOffset});
            }
            break;
        case VertexLayoutType::NORMAL:
            {
                inputLayouts.push_back({g_inputLayoutNameNORMAL, 0, RHIDataFormat::R32G32B32_FLOAT, vertexLayoutOffset});
            }
            break;
        case VertexLayoutType::UV:
            {
                inputLayouts.push_back({g_inputLayoutNameTEXCOORD, 0, RHIDataFormat::R32G32_FLOAT, vertexLayoutOffset});
            }
            break;
        }

        vertexLayoutOffset += vertexLayout.byteSize;   
    }

    return inputLayouts;
}
