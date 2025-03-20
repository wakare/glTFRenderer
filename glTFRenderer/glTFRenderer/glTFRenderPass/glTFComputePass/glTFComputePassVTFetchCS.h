#pragma once
#include "glTFComputePassBase.h"

class IRHIBufferAllocation;

struct ComputePassVTFetchUAVOutput
{
    inline static std::string Name = "VT_FETCH_UAV_OUTPUT_REGISTER_INDEX";

    unsigned data[4];
};

class glTFComputePassVTFetchCS : public glTFComputePassBase
{
public:
    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual DispatchCount GetDispatchCount(glTFRenderResourceManager& resource_manager) const override;

    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    const std::vector<ComputePassVTFetchUAVOutput>& GetFeedbackOutputDataAndReset();
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;

protected:
    std::vector<ComputePassVTFetchUAVOutput> m_uav_output_buffer_data;
    std::shared_ptr<IRHIBufferAllocation> m_readback_buffer;
};

