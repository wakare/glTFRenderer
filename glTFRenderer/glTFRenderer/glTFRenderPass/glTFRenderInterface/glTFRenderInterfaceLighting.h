#pragma once
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHIRootSignatureHelper;

class glTFRenderInterfaceLighting : public glTFRenderInterfaceBaseWithDefaultImpl
{
public:
    glTFRenderInterfaceLighting();
    bool UpdateLightInfo(const glTFLightBase& light);

    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned frame_index) override;

protected:
    ConstantBufferPerLightDraw m_light_buffer_data;

    std::map<unsigned, unsigned> light_indices;
    std::vector<LightInfo> light_infos;
};

