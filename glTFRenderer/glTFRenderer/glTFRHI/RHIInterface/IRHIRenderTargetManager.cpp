#include "IRHIRenderTargetManager.h"

#include <utility>

bool IRHIRenderTargetManager::RegisterRenderTargetWithTag(const std::string& render_target_tag,
                                                          std::shared_ptr<IRHIRenderTarget> render_target, unsigned back_buffer_index)
{
    const std::string resource_key = render_target_tag + std::to_string(back_buffer_index);
    RETURN_IF_FALSE (m_registerRenderTargets.find(resource_key) == m_registerRenderTargets.end())

    m_registerRenderTargets[resource_key] = std::move(render_target);
    return true;
}

std::shared_ptr<IRHIRenderTarget> IRHIRenderTargetManager::GetRenderTargetWithTag(
    const std::string& render_target_tag, unsigned back_buffer_index) const
{
    const std::string resource_key = render_target_tag + std::to_string(back_buffer_index);
    
    const auto it = m_registerRenderTargets.find(resource_key);
    return it == m_registerRenderTargets.end() ? std::shared_ptr<IRHIRenderTarget>(nullptr) : it->second;
}
