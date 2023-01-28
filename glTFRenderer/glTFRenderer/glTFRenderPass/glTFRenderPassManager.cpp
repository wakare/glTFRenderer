#include "glTFRenderPassManager.h"

#include <assert.h>

glTFRenderPassManager::glTFRenderPassManager(glTFWindow& window)
    : m_window(window)
{
    const bool inited = InitRenderPassManager();
    assert(inited);
}

bool glTFRenderPassManager::InitRenderPassManager()
{
    m_resourceManager.reset(new glTFRenderResourceManager());
    if (!m_resourceManager->InitResourceManager(m_window))
    {
        return false;
    }
    
    return true;
}

void glTFRenderPassManager::AddRenderPass(std::unique_ptr<glTFRenderPassBase>&& pass)
{
    m_passes.push_back(std::move(pass));
}

void glTFRenderPassManager::InitAllPass()
{
    for (const auto& pass : m_passes)
    {
        pass->InitPass(*m_resourceManager);
    }
}

void glTFRenderPassManager::RenderAllPass()
{
    m_resourceManager->TickFrameIndex();
    for (const auto& pass : m_passes)
    {
        pass->RenderPass(*m_resourceManager);
    }
}
