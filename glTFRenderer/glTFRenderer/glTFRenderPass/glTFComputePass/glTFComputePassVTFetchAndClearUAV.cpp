#include "glTFComputePassVTFetchAndClearUAV.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceTextureTable.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"

ALIGN_FOR_CBV_STRUCT struct VT_FETCH_OUTPUT_INFO
{
    inline static std::string Name = "VT_FETCH_OUTPUT_INFO_CBV_INDEX";
    
    unsigned texture_count;
    unsigned texture_width;
    unsigned texture_height;
};

const char* glTFComputePassVTFetchAndClearUAV::PassName()
{
    return "glTFComputePass_VTFetchAndClearUAV";
}

bool glTFComputePassVTFetchAndClearUAV::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>>("CLEAR_UINT_TEXTURES_REGISTER_INDEX"));

    auto feedback_texture_size = VirtualTextureSystem::GetVTFeedbackTextureSize(resource_manager);
    size_t fetch_uav_size = feedback_texture_size.first * feedback_texture_size.second * sizeof(ComputePassVTFetchUAVOutput);
    m_uav_output_buffer_data.resize(feedback_texture_size.first * feedback_texture_size.second);

    RHIBufferDesc readback_buffer_desc
    {
        L"VT_FEEDBACK_READBACK_BUFFER",
        fetch_uav_size,
        1,
        1,
        RHIBufferType::Readback,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COMMON,
        static_cast<RHIResourceUsageFlags>(RUF_READBACK | RUF_TRANSFER_DST),
    };
    resource_manager.GetMemoryManager().AllocateBufferMemory(resource_manager.GetDevice(), readback_buffer_desc, m_readback_buffer);

    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<ComputePassVTFetchUAVOutput, RHIViewType::RVT_UAV>>(ComputePassVTFetchUAVOutput::Name.c_str(), fetch_uav_size));
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<VT_FETCH_OUTPUT_INFO>>());
    
    GetRenderInterface<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>>()->AddTexture(m_uav_uint_textures);
    
    return true;
}

bool glTFComputePassVTFetchAndClearUAV::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))
    auto feedback_texture_size = VirtualTextureSystem::GetVTFeedbackTextureSize(resource_manager);
    
    VT_FETCH_OUTPUT_INFO info;
    info.texture_count = m_uav_uint_textures.size();
    info.texture_width = feedback_texture_size.first;
    info.texture_height = feedback_texture_size.second;
    
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<VT_FETCH_OUTPUT_INFO>>()->UploadBuffer(resource_manager, &info, 0, sizeof(info));
    
    return true;
}

bool glTFComputePassVTFetchAndClearUAV::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))
    
    GetComputePipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\ComputeShader\VTFetchAndClearUAVCS.hlsl)", RHIShaderType::Compute, "main");
    
    return true;
}

DispatchCount glTFComputePassVTFetchAndClearUAV::GetDispatchCount(glTFRenderResourceManager& resource_manager) const
{
    auto feedback_texture_size = VirtualTextureSystem::GetVTFeedbackTextureSize(resource_manager);
    
    return {feedback_texture_size.first / 8, feedback_texture_size.second / 8, 1};
}

bool glTFComputePassVTFetchAndClearUAV::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    // Add uav barrier
    for (const auto& texture : m_uav_uint_textures)
    {
        RHIUtils::Instance().AddUAVBarrier(resource_manager.GetCommandListForRecord(), *texture);
    }
    
    return true;
}

bool glTFComputePassVTFetchAndClearUAV::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    // Read back
    auto& output_buffer= GetRenderInterface<glTFRenderInterfaceStructuredBuffer<ComputePassVTFetchUAVOutput, RHIViewType::RVT_UAV>>()->GetBuffer();
    RHIUtils::Instance().CopyBuffer(resource_manager.GetCommandListForRecord(), *m_readback_buffer->m_buffer, 0, output_buffer, 0, m_readback_buffer->m_buffer->GetBufferDesc().width);
    resource_manager.GetMemoryManager().DownloadBufferData(*m_readback_buffer, m_uav_output_buffer_data.data(), m_uav_output_buffer_data.size() * sizeof(ComputePassVTFetchUAVOutput));
    
    return true;
}

bool glTFComputePassVTFetchAndClearUAV::UpdateUAVTextures(const std::vector<std::shared_ptr<IRHITexture>>& uav_textures)
{
    m_uav_uint_textures = uav_textures;
    return true;
}

const std::vector<ComputePassVTFetchUAVOutput>& glTFComputePassVTFetchAndClearUAV::GetFeedbackOutputDataAndReset()
{
    return m_uav_output_buffer_data;
}
