#include "IRHISemaphore.h"

bool RHISemaphoreNull::InitSemaphore(IRHIDevice& device)
{
    return true;
}

bool RHISemaphoreNull::Release(glTFRenderResourceManager&)
{
    return true;
}
