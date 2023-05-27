#include "glTFRenderResourceManager.h"

#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFWindow/glTFWindow.h"

#define EXIT_WHEN_FALSE(x) if (!(x)) {assert(false); return false;}

constexpr size_t backBufferCount = 3;

glTFRenderResourceManager::glTFRenderResourceManager()
    : m_currentBackBufferIndex(0)
{
}

bool glTFRenderResourceManager::InitResourceManager(glTFWindow& window)
{
    m_factory = RHIResourceFactory::CreateRHIResource<IRHIFactory>();
    EXIT_WHEN_FALSE(m_factory->InitFactory())
    
    m_device = RHIResourceFactory::CreateRHIResource<IRHIDevice>();
    EXIT_WHEN_FALSE(m_device->InitDevice(*m_factory))

    m_commandQueue = RHIResourceFactory::CreateRHIResource<IRHICommandQueue>();
    EXIT_WHEN_FALSE(m_commandQueue->InitCommandQueue(*m_device))
    
    m_swapchain = RHIResourceFactory::CreateRHIResource<IRHISwapChain>();
    EXIT_WHEN_FALSE(m_swapchain->InitSwapChain(*m_factory, *m_commandQueue, window.GetWidth(), window.GetHeight(), false,  window))
    UpdateCurrentBackBufferIndex();
    
    m_commandAllocators.resize(backBufferCount);
    for (size_t i = 0; i < backBufferCount; ++i)
    {
        m_commandAllocators[i] = RHIResourceFactory::CreateRHIResource<IRHICommandAllocator>();
        m_commandAllocators[i]->InitCommandAllocator(*m_device, RHICommandAllocatorType::DIRECT);
    }

    // Use first allocator to init command list
    m_commandList = RHIResourceFactory::CreateRHIResource<IRHICommandList>();
    m_commandList->InitCommandList(*m_device, *m_commandAllocators[0]);

    m_fences.resize(backBufferCount);
    for (size_t i = 0; i < backBufferCount; ++i)
    {
        m_fences[i] = RHIResourceFactory::CreateRHIResource<IRHIFence>();
        m_fences[i]->InitFence(*m_device);
    }

    m_renderTargetManager = RHIResourceFactory::CreateRHIResource<IRHIRenderTargetManager>();
    m_renderTargetManager->InitRenderTargetManager(*m_device, 100);

    RHIRenderTargetClearValue clearValue;
    clearValue.clearColor = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};
    m_swapchainRTs = m_renderTargetManager->CreateRenderTargetFromSwapChain(*m_device, *m_swapchain, clearValue);

    RHIRenderTargetClearValue depthClearValue{};
    depthClearValue.clearDS.clearDepth = 1.0f;
    depthClearValue.clearDS.clearStencilValue = 0;
    m_depthTexture = m_renderTargetManager->CreateRenderTarget(*m_device, RHIRenderTargetType::DSV, RHIDataFormat::R32_TYPELESS,
                                                               RHIDataFormat::D32_FLOAT, IRHIRenderTargetDesc{GetSwapchain().GetWidth(), GetSwapchain().GetHeight(), false, depthClearValue, "ResourceManager_DepthRT"});
    
    return true;
}

IRHIFactory& glTFRenderResourceManager::GetFactory()
{
    return *m_factory;
}

IRHIDevice& glTFRenderResourceManager::GetDevice()
{
    return *m_device;
}

IRHISwapChain& glTFRenderResourceManager::GetSwapchain()
{
    return *m_swapchain;
}

IRHICommandQueue& glTFRenderResourceManager::GetCommandQueue()
{
    return *m_commandQueue;
}

IRHICommandList& glTFRenderResourceManager::GetCommandList()
{
    return *m_commandList;
}

IRHIRenderTargetManager& glTFRenderResourceManager::GetRenderTargetManager()
{
    return *m_renderTargetManager;
}

IRHICommandAllocator& glTFRenderResourceManager::GetCurrentFrameCommandAllocator()
{
    return *m_commandAllocators[m_currentBackBufferIndex % backBufferCount];
}

IRHIFence& glTFRenderResourceManager::GetCurrentFrameFence()
{
    return *m_fences[m_currentBackBufferIndex % backBufferCount];
}

IRHIRenderTarget& glTFRenderResourceManager::GetCurrentFrameSwapchainRT()
{
    return *m_swapchainRTs[m_currentBackBufferIndex % backBufferCount];
}

IRHIRenderTarget& glTFRenderResourceManager::GetDepthRT()
{
    return *m_depthTexture;
}

