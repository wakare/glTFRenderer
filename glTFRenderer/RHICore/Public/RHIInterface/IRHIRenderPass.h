#pragma once
#include "RHIInterface/IRHIDevice.h"
#include "RHICommon.h"

struct RHIRenderPassAttachment
{
    RHIDataFormat format;
    RHISampleCount sample_count;
    RHIAttachmentLoadOp load_op;
    RHIAttachmentStoreOp store_op;
    RHIAttachmentLoadOp stencil_load_op;
    RHIAttachmentStoreOp stencil_store_op;
    RHIImageLayout initial_layout;
    RHIImageLayout final_layout;
};

struct RHIRenderPassInfo
{
    std::vector<RHIRenderPassAttachment> attachments;
    std::vector<RHISubPassInfo> subpass_infos;
    std::vector<RHISubPassDependency> sub_pass_dependencies;
};

class RHICORE_API IRHIRenderPass : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRenderPass)

    virtual bool InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info) = 0;
};
