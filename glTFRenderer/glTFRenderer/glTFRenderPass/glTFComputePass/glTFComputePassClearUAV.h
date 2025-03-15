#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceTextureTable.h"

class glTFComputePassClearUAV : public glTFComputePassBase
{
public:
    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual DispatchCount GetDispatchCount(glTFRenderResourceManager& resource_manager) const override;
    
    bool AddUAVTextures(const std::vector<std::shared_ptr<IRHITexture>>& uav_textures);

protected:
    std::vector<std::shared_ptr<IRHITexture>> m_uav_uint_textures;
    std::vector<std::shared_ptr<IRHITexture>> m_uav_float_textures;

    std::shared_ptr<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>> m_uav_uint_texture_table;
    std::shared_ptr<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>> m_uav_float_texture_table;
};
