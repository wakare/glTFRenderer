#pragma once

#include <cstddef>
#include <map>

#include "RenderPass.h"

namespace RenderGraphExecutionPolicy
{
    std::size_t ComputeValidationMessageHash(const RenderPass::DrawValidationResult& result);

    bool ShouldEmitValidationLog(
        std::map<RendererInterface::RenderGraphNodeHandle, unsigned long long>& io_last_log_frame,
        std::map<RendererInterface::RenderGraphNodeHandle, std::size_t>& io_last_message_hash,
        RendererInterface::RenderGraphNodeHandle node_handle,
        const RenderPass::DrawValidationResult& result,
        unsigned long long frame_index,
        unsigned log_interval_frames);

    bool ShouldSkipExecution(const RenderPass::DrawValidationResult& result, bool skip_execution_on_warning);
}
