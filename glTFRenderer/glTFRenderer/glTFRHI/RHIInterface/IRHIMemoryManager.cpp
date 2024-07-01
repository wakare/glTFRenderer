#include "IRHIMemoryManager.h"

IRHIDescriptorHeap& IRHIMemoryManager::GetDescriptorHeap(RHIDescriptorHeapType type) const
{
    switch (type) {
    case RHIDescriptorHeapType::CBV_SRV_UAV:
        return *m_CBV_SRV_UAV_heap;
        break;
    case RHIDescriptorHeapType::RTV:
        return *m_RTV_heap;
        break;
    case RHIDescriptorHeapType::DSV:
        return *m_DSV_heap;
        break;
    }

    GLTF_CHECK(false);
    return *m_CBV_SRV_UAV_heap;
}
