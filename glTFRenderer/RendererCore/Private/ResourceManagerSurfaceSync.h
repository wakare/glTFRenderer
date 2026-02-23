#pragma once

#include "ResourceManager.h"

class ResourceManagerSurfaceResourceRebuilder
{
public:
    static bool ResizeSwapchainIfNeeded(ResourceManager& manager, unsigned width, unsigned height);
    static bool ResizeWindowDependentRenderTargets(ResourceManager& manager, unsigned width, unsigned height, bool assume_gpu_idle);
};

class ResourceManagerSurfaceResizeCoordinator
{
public:
    static RendererInterface::WindowSurfaceSyncResult Sync(ResourceManager& manager, unsigned window_width, unsigned window_height);
    static void NotifySwapchainAcquireFailure(ResourceManager& manager);
    static void NotifySwapchainPresentFailure(ResourceManager& manager);
    static void InvalidateSwapchainResizeRequest(ResourceManager& manager);
};
