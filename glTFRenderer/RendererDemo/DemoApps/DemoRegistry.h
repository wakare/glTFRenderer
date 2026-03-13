#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "DemoAppModelViewer.h"
#include "DemoAppModelViewerFrostedGlass.h"
#include "DemoTriangleApp.h"

namespace DemoRegistry
{
    struct DemoDescriptor
    {
        const char* command_name;
        const char* display_name;
        std::unique_ptr<DemoBase> (*create)();
    };

    template <typename DemoType>
    std::unique_ptr<DemoBase> CreateDemoInstance()
    {
        return std::make_unique<DemoType>();
    }

    inline const std::array<DemoDescriptor, 3>& GetDemoDescriptors()
    {
        static const std::array<DemoDescriptor, 3> k_demo_descriptors{{
            {"DemoTriangleApp", "DemoTriangleApp", &CreateDemoInstance<DemoTriangleApp>},
            {"DemoAppModelViewer", "DemoAppModelViewer", &CreateDemoInstance<DemoAppModelViewer>},
            {"DemoAppModelViewerFrostedGlass", "DemoAppModelViewerFrostedGlass", &CreateDemoInstance<DemoAppModelViewerFrostedGlass>},
        }};
        return k_demo_descriptors;
    }

    inline const DemoDescriptor* FindDemoDescriptorByCommandName(std::string_view command_name)
    {
        const auto& demo_descriptors = GetDemoDescriptors();
        for (const DemoDescriptor& descriptor : demo_descriptors)
        {
            if (std::string_view(descriptor.command_name) == command_name)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    inline std::unique_ptr<DemoBase> CreateDemoByCommandName(std::string_view command_name)
    {
        const DemoDescriptor* descriptor = FindDemoDescriptorByCommandName(command_name);
        if (!descriptor || !descriptor->create)
        {
            return nullptr;
        }
        return descriptor->create();
    }
}
