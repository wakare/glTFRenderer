#include "glTFComputePassClearUAV.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceTextureTable.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

ALIGN_FOR_CBV_STRUCT struct CLEAR_UAV_INFO
{
    inline static std::string Name = "CLEAR_UAV_INFO_CBV_INDEX";
    
    unsigned uint_texture_count;
    unsigned float_texture_count;
};

const char* glTFComputePassClearUAV::PassName()
{
    return "glTFComputePass_ClearUAV";
}

bool glTFComputePassClearUAV::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitRenderInterface(resource_manager))

    //if (!m_uav_uint_textures.empty())
    {
        m_uav_uint_texture_table = std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>>("CLEAR_UINT_TEXTURES_REGISTER_INDEX");
        m_uav_uint_texture_table->AddTexture(m_uav_uint_textures);
        AddRenderInterface(m_uav_uint_texture_table);    
    }
    
    //if (!m_uav_float_textures.empty())
    {
        m_uav_float_texture_table = std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>>("CLEAR_FLOAT_TEXTURES_REGISTER_INDEX");
        m_uav_float_texture_table->AddTexture(m_uav_float_textures);
        AddRenderInterface(m_uav_float_texture_table);
    }
    
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<CLEAR_UAV_INFO>>());
    
    return true;
}

bool glTFComputePassClearUAV::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    CLEAR_UAV_INFO info;
    info.uint_texture_count = m_uav_uint_textures.size();
    info.float_texture_count = m_uav_float_textures.size();
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<CLEAR_UAV_INFO>>()->UploadBuffer(resource_manager, &info, 0, sizeof(info));

    return true;
}

bool glTFComputePassClearUAV::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\ClearUAVCS.hlsl)", RHIShaderType::Compute, "main");
    
    return true;
}

DispatchCount glTFComputePassClearUAV::GetDispatchCount(glTFRenderResourceManager& resource_manager) const
{
    return {resource_manager.GetSwapChain().GetWidth() / 8, resource_manager.GetSwapChain().GetHeight() / 8, 1};
}

bool glTFComputePassClearUAV::AddUAVTextures(const std::vector<std::shared_ptr<IRHITexture>>& uav_textures)
{
    for (const auto& texture : uav_textures)
    {
        if (IsFloatDataFormat(texture->GetTextureDesc().GetDataFormat()))
        {
            m_uav_float_textures.push_back(texture);
        }
        else if (IsUintDataFormat(texture->GetTextureDesc().GetDataFormat()))
        {
            m_uav_uint_textures.push_back(texture);    
        }
    }
    
    return true;
}

