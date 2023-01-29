#include "glTFRenderPassManager.h"

#include <assert.h>

#include "../glTFRHI/RHIUtils.h"
#include "../glTFUtils/glTFLog.h"

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
    m_resourceManager->GetCurrentFrameFence().WaitUtilSignal();
    LOG_FORMAT_FLUSH("[DEBUG] Render frame %d finished...\n", m_resourceManager->GetCurrentFrameIndex())
    
    // Reset command allocator when previous frame executed finish...
    RHIUtils::Instance().ResetCommandAllocator(m_resourceManager->GetCurrentFrameCommandAllocator());
    
    for (const auto& pass : m_passes)
    {
        pass->RenderPass(*m_resourceManager);
    }

    RHIUtils::Instance().CloseCommandList(m_resourceManager->GetCommandList()); 
    
    RHIUtils::Instance().ExecuteCommandList(m_resourceManager->GetCommandList(), m_resourceManager->GetCommandQueue());
    m_resourceManager->GetCurrentFrameFence().SignalWhenCommandQueueFinish(m_resourceManager->GetCommandQueue());
    
    RHIUtils::Instance().Present(m_resourceManager->GetSwapchain());
    
    m_resourceManager->UpdateFrameIndex();
}
