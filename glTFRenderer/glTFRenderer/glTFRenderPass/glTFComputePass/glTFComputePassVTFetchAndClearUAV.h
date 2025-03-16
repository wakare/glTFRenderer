#pragma once
#include "glTFComputePassBase.h"

class IRHIBufferAllocation;

struct ComputePassVTFetchUAVOutput
{
    inline static std::string Name = "VT_FETCH_UAV_OUTPUT_REGISTER_INDEX";

    unsigned data[4];
};

class glTFComputePassVTFetchAndClearUAV : public glTFComputePassBase
{
public:
    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual DispatchCount GetDispatchCount(glTFRenderResourceManager& resource_manager) const override;

    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    bool UpdateUAVTextures(const std::vector<std::shared_ptr<IRHITexture>>& uav_textures);
    const std::vector<ComputePassVTFetchUAVOutput>& GetFeedbackOutputDataAndReset();
    
protected:
    std::vector<std::shared_ptr<IRHITexture>> m_uav_uint_textures;
    std::vector<ComputePassVTFetchUAVOutput> m_uav_output_buffer_data;
    std::shared_ptr<IRHIBufferAllocation> m_readback_buffer;
};

