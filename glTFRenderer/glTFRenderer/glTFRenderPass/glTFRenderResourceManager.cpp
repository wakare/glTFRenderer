#include "glTFRenderResourceManager.h"

#include "glTFRenderMaterialManager.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFWindow/glTFWindow.h"

#define EXIT_WHEN_FALSE(x) if (!(x)) {assert(false); return false;}

constexpr size_t backBufferCount = 3;

glTFRenderResourceManager::glTFRenderResourceManager()
    : m_material_manager(std::make_shared<glTFRenderMaterialManager>())
    , m_currentBackBufferIndex(0)
{
}

bool glTFRenderResourceManager::InitResourceManager(glTFWindow& window)
{
    m_factory = RHIResourceFactory::CreateRHIResource<IRHIFactory>();
    EXIT_WHEN_FALSE(m_factory->InitFactory())
    
    m_device = RHIResourceFactory::CreateRHIResource<IRHIDevice>();
    EXIT_WHEN_FALSE(m_device->InitDevice(*m_factory))

    m_command_queue = RHIResourceFactory::CreateRHIResource<IRHICommandQueue>();
    EXIT_WHEN_FALSE(m_command_queue->InitCommandQueue(*m_device))
    
    m_swapchain = RHIResourceFactory::CreateRHIResource<IRHISwapChain>();
    EXIT_WHEN_FALSE(m_swapchain->InitSwapChain(*m_factory, *m_command_queue, window.GetWidth(), window.GetHeight(), false,  window))
    UpdateCurrentBackBufferIndex();
    
    m_command_allocators.resize(backBufferCount);
    m_command_lists.resize(backBufferCount);
    m_command_list_record_state.resize(backBufferCount);
    
    for (size_t i = 0; i < backBufferCount; ++i)
    {
        m_command_allocators[i] = RHIResourceFactory::CreateRHIResource<IRHICommandAllocator>();
        m_command_allocators[i]->InitCommandAllocator(*m_device, RHICommandAllocatorType::DIRECT);
        
        m_command_lists[i] = RHIResourceFactory::CreateRHIResource<IRHICommandList>();
        m_command_lists[i]->InitCommandList(*m_device, *m_command_allocators[i]);
        m_command_list_record_state[i] = false;
    }

    m_fences.resize(backBufferCount);
    for (size_t i = 0; i < backBufferCount; ++i)
    {
        m_fences[i] = RHIResourceFactory::CreateRHIResource<IRHIFence>();
        m_fences[i]->InitFence(*m_device);
    }

    m_render_target_manager = RHIResourceFactory::CreateRHIResource<IRHIRenderTargetManager>();
    m_render_target_manager->InitRenderTargetManager(*m_device, 100);

    RHIRenderTargetClearValue clearValue;
    clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM_SRGB;
    clearValue.clear_color = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};
    m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *m_swapchain, clearValue);

    RHIRenderTargetClearValue depth_clear_value{};
    depth_clear_value.clear_format = RHIDataFormat::D32_FLOAT;
    depth_clear_value.clearDS.clear_depth = 1.0f;
    depth_clear_value.clearDS.clear_stencil_value = 0;
    m_depth_texture = m_render_target_manager->CreateRenderTarget(*m_device, RHIRenderTargetType::DSV, RHIDataFormat::R32_TYPELESS,
                                                               RHIDataFormat::D32_FLOAT, IRHIRenderTargetDesc{GetSwapchain().GetWidth(), GetSwapchain().GetHeight(), false, depth_clear_value, "ResourceManager_DepthRT"});
    
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
    return *m_command_queue;
}

IRHICommandList& glTFRenderResourceManager::GetCommandListForRecord()
{
    const auto current_frame_index = m_currentBackBufferIndex % backBufferCount;
    auto& command_list = *m_command_lists[current_frame_index];
    auto& command_allocator = *m_command_allocators[current_frame_index];
    if (!m_command_list_record_state[current_frame_index])
    {
        GLTF_CHECK(RHIUtils::Instance().ResetCommandList(command_list, command_allocator, m_current_pass_pso.get()));
        m_command_list_record_state[current_frame_index] = true;
    }
    
    return command_list;
}

void glTFRenderResourceManager::CloseCommandListAndExecute(bool wait)
{
    const auto current_frame_index = m_currentBackBufferIndex % backBufferCount;
    auto& command_list = *m_command_lists[current_frame_index];
    GLTF_CHECK(m_command_list_record_state[current_frame_index]);
    
    GLTF_CHECK(RHIUtils::Instance().CloseCommandList(command_list)); 
    GLTF_CHECK(RHIUtils::Instance().ExecuteCommandList(command_list, GetCommandQueue()));
    GLTF_CHECK(GetCurrentFrameFence().SignalWhenCommandQueueFinish(GetCommandQueue()));
    if (wait)
    {
        GetCurrentFrameFence().WaitUtilSignal();
    }
    
    m_command_list_record_state[current_frame_index] = false;
}

void glTFRenderResourceManager::WaitLastFrameFinish()
{
    GetCurrentFrameFence().WaitUtilSignal();
}

void glTFRenderResourceManager::ResetCommandAllocator()
{
    RHIUtils::Instance().ResetCommandAllocator(GetCurrentFrameCommandAllocator());
}

IRHIRenderTargetManager& glTFRenderResourceManager::GetRenderTargetManager()
{
    return *m_render_target_manager;
}

IRHICommandAllocator& glTFRenderResourceManager::GetCurrentFrameCommandAllocator()
{
    return *m_command_allocators[m_currentBackBufferIndex % backBufferCount];
}

IRHIFence& glTFRenderResourceManager::GetCurrentFrameFence()
{
    return *m_fences[m_currentBackBufferIndex % backBufferCount];
}

IRHIRenderTarget& glTFRenderResourceManager::GetCurrentFrameSwapchainRT()
{
    return *m_swapchain_RTs[m_currentBackBufferIndex % backBufferCount];
}

IRHIRenderTarget& glTFRenderResourceManager::GetDepthRT()
{
    return *m_depth_texture;
}

glTFRenderMaterialManager& glTFRenderResourceManager::GetMaterialManager()
{
    return *m_material_manager;
}

bool glTFRenderResourceManager::ApplyMaterial(IRHIDescriptorHeap& descriptor_heap, glTFUniqueID material_ID, unsigned slot_index, bool isGraphicsPipeline)
{
    return m_material_manager->ApplyMaterialRenderResource(*this, descriptor_heap, material_ID, slot_index, isGraphicsPipeline);
}

void glTFRenderResourceManager::SetCurrentPSO(std::shared_ptr<IRHIPipelineStateObject> pso)
{
    if (m_current_pass_pso != pso)
    {
        CloseCommandListAndExecute(false);    
    }
    
    m_current_pass_pso = pso;
}

