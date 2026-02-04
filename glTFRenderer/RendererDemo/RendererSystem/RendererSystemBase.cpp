#include "RendererSystemBase.h"

bool RendererSystemBase::FinalizeModule(RendererInterface::ResourceOperator& resource_operator)
{
    for (auto& module : m_modules)
    {
        module->FinalizeModule(resource_operator);
    }
    return true;
}