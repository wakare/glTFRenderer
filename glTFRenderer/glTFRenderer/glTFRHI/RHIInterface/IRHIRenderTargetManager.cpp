#include "IRHIRenderTargetManager.h"

#include <utility>

bool IRHIRenderTargetManager::RegisterRenderTargetWithTag(const std::string& renderTargetTag,
                                                          std::shared_ptr<IRHIRenderTarget> renderTarget)
{
    RETURN_IF_FALSE (m_registerRenderTargets.find(renderTargetTag) == m_registerRenderTargets.end())

    m_registerRenderTargets[renderTargetTag] = std::move(renderTarget);
    return true;
}

std::shared_ptr<IRHIRenderTarget> IRHIRenderTargetManager::GetRenderTargetWithTag(
    const std::string& renderTargetTag) const
{
    const auto it = m_registerRenderTargets.find(renderTargetTag);
    return it == m_registerRenderTargets.end() ? std::shared_ptr<IRHIRenderTarget>(nullptr) : it->second;
}
