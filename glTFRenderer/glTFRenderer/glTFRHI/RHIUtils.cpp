#include "RHIUtils.h"

#include "RHIResourceFactoryImpl.hpp"

std::shared_ptr<RHIUtils> RHIUtils::g_instance = nullptr;

RHIUtils& RHIUtils::Instance()
{
    if (!g_instance)
    {
        g_instance = RHIResourceFactory::CreateRHIResource<RHIUtils>();
    }

    return *g_instance;
}

void RHIUtils::ResetInstance()
{
    g_instance = RHIResourceFactory::CreateRHIResource<RHIUtils>();
}
