#include "glTFComputePassVTFetchCS.h"

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

glTFComputePassVTFetchCS::glTFComputePassVTFetchCS(unsigned virtual_texture_id)
    : m_virtual_texture_id(virtual_texture_id)
{
    
}

const char* glTFComputePassVTFetchCS::PassName()
{
    return "glTFComputePass_VTFetchCS";
}

bool glTFComputePassVTFetchCS::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::SRV>>("CLEAR_UINT_TEXTURES_REGISTER_INDEX"));

    auto feedback_texture_size = VirtualTextureSystem::GetVTFeedbackTextureSize(resource_manager.GetSwapChain().GetWidth(), resource_manager.GetSwapChain().GetHeight());
    
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

    std::shared_ptr<IRHITexture> feed_back_texture = GetResourceTexture(GetVTFeedBackId(m_virtual_texture_id));
    GetRenderInterface<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::SRV>>()->AddTexture({feed_back_texture});
    
    return true;
}

bool glTFComputePassVTFetchCS::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))
    auto feedback_texture_size = VirtualTextureSystem::GetVTFeedbackTextureSize(resource_manager.GetSwapChain().GetWidth(), resource_manager.GetSwapChain().GetHeight());
    
    VT_FETCH_OUTPUT_INFO info;
    info.texture_count = 1;
    info.texture_width = feedback_texture_size.first;
    info.texture_height = feedback_texture_size.second;
    
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<VT_FETCH_OUTPUT_INFO>>()->UploadBuffer(resource_manager, &info, 0, sizeof(info));
    
    return true;
}

bool glTFComputePassVTFetchCS::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))
    
    GetComputePipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\ComputeShader\VTFetchCS.hlsl)", RHIShaderType::Compute, "main");
    
    return true;
}

DispatchCount glTFComputePassVTFetchCS::GetDispatchCount(glTFRenderResourceManager& resource_manager) const
{
    auto feedback_texture_size = VirtualTextureSystem::GetVTFeedbackTextureSize(resource_manager.GetSwapChain().GetWidth(), resource_manager.GetSwapChain().GetHeight());
    
    return {feedback_texture_size.first / 8, feedback_texture_size.second / 8, 1};
}

bool glTFComputePassVTFetchCS::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    return true;
}

bool glTFComputePassVTFetchCS::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    // Read back
    auto& output_buffer= GetRenderInterface<glTFRenderInterfaceStructuredBuffer<ComputePassVTFetchUAVOutput, RHIViewType::RVT_UAV>>()->GetBuffer();
    RHIUtils::Instance().CopyBuffer(resource_manager.GetCommandListForRecord(), *m_readback_buffer->m_buffer, 0, output_buffer, 0, m_readback_buffer->m_buffer->GetBufferDesc().width);
    resource_manager.GetMemoryManager().DownloadBufferData(*m_readback_buffer, m_uav_output_buffer_data.data(), m_uav_output_buffer_data.size() * sizeof(ComputePassVTFetchUAVOutput));

    SetFeedBackDataValid(true);
    
    return true;
}

bool glTFComputePassVTFetchCS::IsFeedBackDataValid() const
{
    return m_feedback_data_valid;
}

void glTFComputePassVTFetchCS::SetFeedBackDataValid(bool valid)
{
    m_feedback_data_valid = valid;
}

const std::vector<ComputePassVTFetchUAVOutput>& glTFComputePassVTFetchCS::GetFeedbackOutputData()
{
    return m_uav_output_buffer_data;
}

bool glTFComputePassVTFetchCS::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitResourceTable(resource_manager))
    const auto& vt_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->GetVTFeedbackTextureSize(resource_manager.GetSwapChain().GetWidth(), resource_manager.GetSwapChain().GetHeight());
    
    RHITextureDesc feed_back_desc = RHITextureDesc::MakeVirtualTextureFeedbackDesc(resource_manager, vt_size.first, vt_size.second);
    AddImportTextureResource(GetVTFeedBackId(m_virtual_texture_id), feed_back_desc,
        {
            feed_back_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV,
        });
    
    return true;
}
