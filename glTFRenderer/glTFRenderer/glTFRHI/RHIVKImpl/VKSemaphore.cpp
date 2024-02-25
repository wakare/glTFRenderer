#include "VKSemaphore.h"

bool VKSemaphore::InitSemaphore(VkSemaphore semaphore)
{
    m_semaphore = semaphore;
    return true;
}

VkSemaphore VKSemaphore::GetSemaphore() const
{
    return m_semaphore;
}
