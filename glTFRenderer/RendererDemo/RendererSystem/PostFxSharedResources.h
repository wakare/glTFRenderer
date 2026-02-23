#pragma once

#include "RendererInterface.h"

#include <array>
#include <string>

class PostFxSharedResources
{
public:
    enum class Resolution : unsigned
    {
        Half = 0,
        Quarter = 1,
        Count
    };

    struct PingPongPair
    {
        RendererInterface::RenderTargetHandle ping{NULL_HANDLE};
        RendererInterface::RenderTargetHandle pong{NULL_HANDLE};

        bool IsValid() const
        {
            return ping != NULL_HANDLE && pong != NULL_HANDLE;
        }
    };

    bool Init(RendererInterface::ResourceOperator& resource_operator,
              const std::string& name_prefix,
              RendererInterface::PixelFormat format,
              RendererInterface::RenderTargetClearValue clear_value,
              RendererInterface::ResourceUsage usage)
    {
        if (m_initialized)
        {
            return true;
        }

        m_pairs[ToIndex(Resolution::Half)] = CreatePair(resource_operator, name_prefix, "Half", format, clear_value, usage, 0.5f);
        m_pairs[ToIndex(Resolution::Quarter)] = CreatePair(resource_operator, name_prefix, "Quarter", format, clear_value, usage, 0.25f);

        m_initialized = true;
        for (const auto& pair : m_pairs)
        {
            if (!pair.IsValid())
            {
                m_initialized = false;
                break;
            }
        }
        return m_initialized;
    }

    bool HasInit() const
    {
        if (!m_initialized)
        {
            return false;
        }

        for (const auto& pair : m_pairs)
        {
            if (!pair.IsValid())
            {
                return false;
            }
        }
        return true;
    }

    PingPongPair GetPair(Resolution resolution) const
    {
        return m_pairs[ToIndex(resolution)];
    }

    RendererInterface::RenderTargetHandle GetPing(Resolution resolution) const
    {
        return GetPair(resolution).ping;
    }

    RendererInterface::RenderTargetHandle GetPong(Resolution resolution) const
    {
        return GetPair(resolution).pong;
    }

private:
    static constexpr unsigned kResolutionCount = static_cast<unsigned>(Resolution::Count);

    static constexpr unsigned ToIndex(Resolution resolution)
    {
        return static_cast<unsigned>(resolution);
    }

    PingPongPair CreatePair(RendererInterface::ResourceOperator& resource_operator,
                            const std::string& name_prefix,
                            const char* resolution_name,
                            RendererInterface::PixelFormat format,
                            RendererInterface::RenderTargetClearValue clear_value,
                            RendererInterface::ResourceUsage usage,
                            float scale)
    {
        PingPongPair pair{};
        const std::string ping_name = name_prefix + "_" + resolution_name + "_Ping";
        const std::string pong_name = name_prefix + "_" + resolution_name + "_Pong";

        pair.ping = resource_operator.CreateWindowRelativeRenderTarget(
            ping_name,
            format,
            clear_value,
            usage,
            scale,
            scale,
            1,
            1);

        pair.pong = resource_operator.CreateWindowRelativeRenderTarget(
            pong_name,
            format,
            clear_value,
            usage,
            scale,
            scale,
            1,
            1);

        return pair;
    }

    std::array<PingPongPair, kResolutionCount> m_pairs{};
    bool m_initialized{false};
};
