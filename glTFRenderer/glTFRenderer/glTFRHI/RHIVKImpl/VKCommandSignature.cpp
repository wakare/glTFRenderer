#include "VKCommandSignature.h"

bool VKCommandSignature::InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature)
{
    need_release = true;
    return true;
}

bool VKCommandSignature::Release(glTFRenderResourceManager&)
{
    if (!need_release)
    {
        return true;
    }
    
    need_release = false;
    return true;
}
