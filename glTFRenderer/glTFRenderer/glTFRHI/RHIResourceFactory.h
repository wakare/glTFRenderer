#pragma once
#include <memory>

class RHIResourceFactory
{
public:
    // Actually create rhi class instance based RHI Graphics API type
    template<typename Type>
    static std::shared_ptr<Type> CreateRHIResource();

    static std::shared_ptr<IRHIMemoryManager> CreateRHIMemoryManager();
};

template <typename Type>
std::shared_ptr<Type> RHIResourceFactory::CreateRHIResource()
{
    // No implement, please include RHIResourceFactoryImpl.hpp before create RHI instance
    return nullptr;
}
