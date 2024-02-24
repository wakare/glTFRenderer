#pragma once
#include "RHICommon.h"

struct RHIRenderPassAttachment
{
    RHIDataFormat format;
    RHISampleCount sample_count;
    RHIAttachmentLoadOp load_op;
    RHIAttachmentStoreOp store_op;
};

class IRHIRenderPass
{
public:
    
};
