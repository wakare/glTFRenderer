#include "RHIInterface/IRHISemaphore.h"

bool RHISemaphoreNull::InitSemaphore(IRHIDevice& device)
{
    return true;
}

bool RHISemaphoreNull::Release(IRHIMemoryManager& memory_manager)
{
    return true;
}
