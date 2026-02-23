#include "RenderGraphExecutionPolicy.h"

#include <algorithm>
#include <functional>
#include <string>

namespace
{
    void HashCombine(std::size_t& seed, std::size_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }
}

std::size_t RenderGraphExecutionPolicy::ComputeValidationMessageHash(const RenderPass::DrawValidationResult& result)
{
    std::size_t hash = 0;
    HashCombine(hash, std::hash<bool>{}(result.valid));
    for (const auto& error : result.errors)
    {
        HashCombine(hash, std::hash<std::string>{}(error));
    }
    for (const auto& warning : result.warnings)
    {
        HashCombine(hash, std::hash<std::string>{}(warning));
    }
    return hash;
}

bool RenderGraphExecutionPolicy::ShouldEmitValidationLog(
    std::map<RendererInterface::RenderGraphNodeHandle, unsigned long long>& io_last_log_frame,
    std::map<RendererInterface::RenderGraphNodeHandle, std::size_t>& io_last_message_hash,
    RendererInterface::RenderGraphNodeHandle node_handle,
    const RenderPass::DrawValidationResult& result,
    unsigned long long frame_index,
    unsigned log_interval_frames)
{
    const unsigned normalized_log_interval = (std::max)(1u, log_interval_frames);
    const std::size_t message_hash = ComputeValidationMessageHash(result);
    auto& last_log_frame = io_last_log_frame[node_handle];
    auto& last_message_hash = io_last_message_hash[node_handle];

    const bool message_changed = last_message_hash != message_hash;
    const bool interval_reached = frame_index >= (last_log_frame + normalized_log_interval);
    if (!message_changed && !interval_reached)
    {
        return false;
    }

    last_log_frame = frame_index;
    last_message_hash = message_hash;
    return true;
}

bool RenderGraphExecutionPolicy::ShouldSkipExecution(const RenderPass::DrawValidationResult& result, bool skip_execution_on_warning)
{
    if (!result.valid)
    {
        return true;
    }

    if (!skip_execution_on_warning)
    {
        return false;
    }

    return !result.warnings.empty();
}
