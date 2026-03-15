#include "RendererInterface.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include "InternalResourceHandleTable.h"
#include "RendererSceneCommon.h"
#include "RenderPass.h"
#include "RenderGraphExecutionPolicy.h"
#include "ResourceManager.h"
#include "RHIConfigSingleton.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIUtils.h"
#include "RendererSceneGraph.h"
#include "RenderWindow/glTFWindow.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIInterface/IRHIDescriptorUpdater.h"
#include "RHIInterface/IRHIMemoryManager.h"
#include "RHIInterface/IRHISwapChain.h"
#include "RHIInterface/RHIIndexBuffer.h"
#include <renderdoc/renderdoc_app.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>

namespace RendererInterface
{
    struct RenderGraph::GPUProfilerState
    {
        struct FrameSlot
        {
            bool has_pending_frame_stats{false};
            unsigned query_count{0};
            FrameStats pending_frame_stats{};
        };

        bool supported{false};
        unsigned max_pass_count{512};
        unsigned max_query_count{1024};
        double ticks_per_second{0.0};
        std::vector<FrameSlot> frame_slots{};
    };

    struct RenderGraph::RenderDocCaptureState
    {
        struct PendingCapture
        {
            bool armed{false};
            bool started{false};
            unsigned long long frame_index{0};
            uint32_t capture_count_before{0};
            std::filesystem::path requested_path{};
        };

        struct CompletedCapture
        {
            bool attempted{false};
            bool success{false};
            unsigned long long frame_index{0};
            std::string capture_path{};
            std::string error{};
        };

        bool enabled{false};
        bool required{false};
        bool available{false};
        bool owns_module{false};
        bool overlay_bits_saved{false};
        bool overlay_hidden{false};
        HMODULE module_handle{nullptr};
        RENDERDOC_API_1_7_0* api{nullptr};
        int api_version{0};
        uint32_t saved_overlay_bits{eRENDERDOC_Overlay_None};
        std::string status{};
        PendingCapture pending{};
        CompletedCapture last_result{};
    };

    namespace
    {
        bool g_renderdoc_runtime_loaded_by_app = false;

        std::string WideStringToUtf8(const std::wstring& value)
        {
            if (value.empty())
            {
                return {};
            }

            const int required_length = WideCharToMultiByte(
                CP_UTF8,
                0,
                value.c_str(),
                static_cast<int>(value.size()),
                nullptr,
                0,
                nullptr,
                nullptr);
            if (required_length <= 0)
            {
                return {};
            }

            std::string utf8(static_cast<size_t>(required_length), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                value.c_str(),
                static_cast<int>(value.size()),
                utf8.data(),
                required_length,
                nullptr,
                nullptr);
            return utf8;
        }

        std::string PathToUtf8String(const std::filesystem::path& path)
        {
            return WideStringToUtf8(path.wstring());
        }

        std::string QuoteCommandLineArgument(std::string value)
        {
            if (value.empty())
            {
                return "\"\"";
            }

            const bool needs_quotes = value.find_first_of(" \t\"") != std::string::npos;
            if (!needs_quotes)
            {
                return value;
            }

            std::string quoted{};
            quoted.reserve(value.size() + 2);
            quoted.push_back('"');
            for (char ch : value)
            {
                if (ch == '"')
                {
                    quoted += "\\\"";
                    continue;
                }
                quoted.push_back(ch);
            }
            quoted.push_back('"');
            return quoted;
        }

        std::string BuildRenderDocCaptureTemplate(const std::filesystem::path& capture_path)
        {
            std::filesystem::path template_path = capture_path;
            template_path.replace_extension();
            return PathToUtf8String(template_path);
        }

        std::string QueryRenderDocCapturePath(RENDERDOC_API_1_7_0* api, uint32_t capture_index)
        {
            if (!api)
            {
                return {};
            }

            uint32_t path_length = 0;
            if (api->GetCapture(capture_index, nullptr, &path_length, nullptr) == 0 || path_length == 0)
            {
                return {};
            }

            std::string capture_path(static_cast<size_t>(path_length), '\0');
            if (api->GetCapture(capture_index, capture_path.data(), &path_length, nullptr) == 0 || path_length == 0)
            {
                return {};
            }

            if (!capture_path.empty() && capture_path.back() == '\0')
            {
                capture_path.pop_back();
            }
            return capture_path;
        }

        std::filesystem::path QueryRenderDocVulkanManifestPathFromRegistry(HKEY root_key)
        {
            HKEY implicit_layers_key = nullptr;
            if (RegOpenKeyExW(root_key, L"SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers", 0, KEY_READ, &implicit_layers_key) != ERROR_SUCCESS)
            {
                return {};
            }

            const auto close_key = [&]()
            {
                if (implicit_layers_key)
                {
                    RegCloseKey(implicit_layers_key);
                    implicit_layers_key = nullptr;
                }
            };

            for (DWORD index = 0; ; ++index)
            {
                wchar_t value_name[1024] = {};
                DWORD value_name_length = static_cast<DWORD>(std::size(value_name));
                DWORD value_type = 0;
                DWORD value_data = 0;
                DWORD value_data_size = sizeof(value_data);
                const LSTATUS status = RegEnumValueW(
                    implicit_layers_key,
                    index,
                    value_name,
                    &value_name_length,
                    nullptr,
                    &value_type,
                    reinterpret_cast<LPBYTE>(&value_data),
                    &value_data_size);
                if (status == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                if (status != ERROR_SUCCESS)
                {
                    continue;
                }

                std::filesystem::path manifest_path(std::wstring(value_name, value_name_length));
                const std::wstring file_name = manifest_path.filename().wstring();
                if (_wcsicmp(file_name.c_str(), L"renderdoc.json") == 0)
                {
                    close_key();
                    return manifest_path;
                }
            }

            close_key();
            return {};
        }

        std::filesystem::path QueryRenderDocVulkanManifestPath()
        {
            std::filesystem::path manifest_path = QueryRenderDocVulkanManifestPathFromRegistry(HKEY_CURRENT_USER);
            if (!manifest_path.empty())
            {
                return manifest_path;
            }

            return QueryRenderDocVulkanManifestPathFromRegistry(HKEY_LOCAL_MACHINE);
        }

        std::filesystem::path QueryInstalledRenderDocModulePath()
        {
            const std::filesystem::path manifest_path = QueryRenderDocVulkanManifestPath();
            if (manifest_path.empty())
            {
                return {};
            }

            const std::filesystem::path module_path = manifest_path.parent_path() / "renderdoc.dll";
            std::error_code error_code{};
            if (std::filesystem::is_regular_file(module_path, error_code) && !error_code)
            {
                return module_path;
            }

            return {};
        }

        bool QueryRenderDocVulkanImplementationVersion(const std::filesystem::path& manifest_path, int& out_implementation_version)
        {
            out_implementation_version = 0;
            if (manifest_path.empty())
            {
                return false;
            }

            std::ifstream manifest_stream(manifest_path, std::ios::in | std::ios::binary);
            if (!manifest_stream.is_open())
            {
                return false;
            }

            const std::string manifest_contents{
                std::istreambuf_iterator<char>(manifest_stream),
                std::istreambuf_iterator<char>()};
            const std::string key = "\"implementation_version\"";
            const std::size_t key_pos = manifest_contents.find(key);
            if (key_pos == std::string::npos)
            {
                return false;
            }

            const std::size_t colon_pos = manifest_contents.find(':', key_pos + key.size());
            if (colon_pos == std::string::npos)
            {
                return false;
            }

            std::size_t value_pos = manifest_contents.find_first_not_of(" \t\r\n", colon_pos + 1u);
            if (value_pos == std::string::npos)
            {
                return false;
            }

            const bool quoted_value = manifest_contents[value_pos] == '"';
            if (quoted_value)
            {
                ++value_pos;
            }

            const std::size_t value_end = quoted_value
                ? manifest_contents.find('"', value_pos)
                : manifest_contents.find_first_of(",}\r\n \t", value_pos);
            if (value_end == std::string::npos || value_end <= value_pos)
            {
                return false;
            }

            int implementation_version = 0;
            for (std::size_t index = value_pos; index < value_end; ++index)
            {
                const char ch = manifest_contents[index];
                if (ch < '0' || ch > '9')
                {
                    return false;
                }
                implementation_version = implementation_version * 10 + (ch - '0');
            }

            out_implementation_version = implementation_version;
            return true;
        }

        enum class ResourceKind
        {
            Buffer,
            Texture,
            RenderTarget,
        };

        struct ResourceKey
        {
            ResourceKind kind;
            unsigned long long value;

            bool operator<(const ResourceKey& other) const
            {
                if (kind != other.kind)
                {
                    return kind < other.kind;
                }
                return value < other.value;
            }
        };

        struct ResourceAccessSet
        {
            std::set<ResourceKey> reads;
            std::set<ResourceKey> writes;
        };

        constexpr unsigned char RESOURCE_ACCESS_MASK_READ = 1u << 0;
        constexpr unsigned char RESOURCE_ACCESS_MASK_WRITE = 1u << 1;
        constexpr std::size_t MAX_CROSS_FRAME_HAZARDS_RECORDED = 128u;
        constexpr std::size_t MAX_CROSS_FRAME_PASS_NAMES_RECORDED = 8u;

        using ResourceAccessMaskMap = std::map<unsigned long long, unsigned char>;
        using ResourcePassAccessMap = std::map<unsigned long long, std::pair<std::vector<std::string>, std::vector<std::string>>>;

        inline float ToMilliseconds(
            const std::chrono::steady_clock::time_point& begin,
            const std::chrono::steady_clock::time_point& end)
        {
            return std::chrono::duration<float, std::milli>(end - begin).count();
        }

        struct FrameResourceAccessDiagnosticsData
        {
            ResourceAccessMaskMap access_masks;
            ResourcePassAccessMap pass_accesses;
        };

        const char* ToRenderPassTypeName(RenderPassType type)
        {
            switch (type)
            {
            case RenderPassType::GRAPHICS:
                return "Graphics";
            case RenderPassType::COMPUTE:
                return "Compute";
            case RenderPassType::RAY_TRACING:
                return "RayTracing";
            }
            return "Unknown";
        }

        std::string JoinPassNames(const std::vector<std::string>& pass_names)
        {
            std::string joined;
            for (std::size_t pass_index = 0; pass_index < pass_names.size(); ++pass_index)
            {
                if (pass_index > 0)
                {
                    joined += ", ";
                }
                joined += pass_names[pass_index];
            }
            return joined;
        }

        const char* ToCrossFrameHazardTypeName(
            RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType hazard_type)
        {
            switch (hazard_type)
            {
            case RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType::PREVIOUS_WRITE_CURRENT_READ:
                return "PrevWrite->CurrRead";
            case RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType::PREVIOUS_WRITE_CURRENT_WRITE:
                return "PrevWrite->CurrWrite";
            case RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType::PREVIOUS_READ_CURRENT_WRITE:
                return "PrevRead->CurrWrite";
            }
            return "Unknown";
        }

        const char* ToResourceKindName(ResourceKind kind)
        {
            switch (kind)
            {
            case ResourceKind::Buffer:
                return "Buffer";
            case ResourceKind::Texture:
                return "Texture";
            case ResourceKind::RenderTarget:
                return "RenderTarget";
            }
            return "Unknown";
        }

        bool HasDependency(const RenderGraphNodeDesc& desc, RenderGraphNodeHandle handle)
        {
            return std::find(desc.dependency_render_graph_nodes.begin(),
                desc.dependency_render_graph_nodes.end(),
                handle) != desc.dependency_render_graph_nodes.end();
        }

        void HashCombine(std::size_t& seed, std::size_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }

        unsigned ComputeScaledExtent(unsigned base_extent, float scale, unsigned min_extent)
        {
            const float safe_scale = (std::max)(0.0f, scale);
            const unsigned scaled_extent = static_cast<unsigned>((std::round)(static_cast<float>(base_extent) * safe_scale));
            return (std::max)(min_extent, scaled_extent);
        }

        unsigned ResolveSwapchainImageCount(const ResourceOperator& resource_operator)
        {
            return resource_operator.GetFrameContext().swapchain_image_count;
        }

        unsigned ResolveProfilerSlotCount(const ResourceOperator& resource_operator)
        {
            return resource_operator.GetFrameContext().profiler_slot_count;
        }

        unsigned ResolveCrossFrameComparisonWindowSize(const FrameContextSnapshot& frame_context)
        {
            return frame_context.per_frame_resource_binding_enabled
                ? frame_context.frame_slot_count
                : 1u;
        }

        unsigned ResolveDeferredReleaseLatencyFrames(const FrameContextSnapshot& frame_context)
        {
            return (std::max)(2u, frame_context.frame_slot_count);
        }

        unsigned ResolveDescriptorCacheRetentionFrames(const FrameContextSnapshot& frame_context)
        {
            return (std::max)(2u, frame_context.frame_slot_count * 2u);
        }

        unsigned ResolveRenderPassDescriptorRetentionFrames(const FrameContextSnapshot& frame_context)
        {
            return (std::max)(2u, frame_context.frame_slot_count);
        }

        using DependencyEdgeMap = std::map<RenderGraphNodeHandle, std::set<RenderGraphNodeHandle>>;
        using DependencyEdgeResourceMap = std::map<std::pair<RenderGraphNodeHandle, RenderGraphNodeHandle>, std::vector<ResourceKey>>;

        unsigned long long ResolveRenderTargetResourceIdentity(RenderTargetHandle handle)
        {
            if (handle == NULL_HANDLE)
            {
                return 0ull;
            }

            const auto render_target_allocation = InternalResourceHandleTable::Instance().GetRenderTarget(handle);
            if (!render_target_allocation || !render_target_allocation->m_source)
            {
                return static_cast<unsigned long long>(handle.value);
            }

            return reinterpret_cast<unsigned long long>(render_target_allocation->m_source.get());
        }

        unsigned long long EncodeResourceKey(const ResourceKey& key)
        {
            constexpr unsigned long long kind_shift = 62ull;
            constexpr unsigned long long value_mask = (1ull << kind_shift) - 1ull;
            return (static_cast<unsigned long long>(static_cast<unsigned>(key.kind)) << kind_shift) |
                   (key.value & value_mask);
        }

        void AppendUniquePassName(std::vector<std::string>& pass_names, const std::string& pass_name)
        {
            if (pass_name.empty())
            {
                return;
            }
            if (std::find(pass_names.begin(), pass_names.end(), pass_name) != pass_names.end())
            {
                return;
            }
            if (pass_names.size() >= MAX_CROSS_FRAME_PASS_NAMES_RECORDED)
            {
                return;
            }
            pass_names.push_back(pass_name);
        }

        ResourceAccessSet CollectResourceAccess(const RenderGraphNodeDesc& desc)
        {
            ResourceAccessSet access;

            for (const auto& buffer_pair : desc.draw_info.buffer_resources)
            {
                const auto& binding = buffer_pair.second;
                ResourceKey key{ResourceKind::Buffer, static_cast<unsigned long long>(binding.buffer_handle.value)};

                switch (binding.binding_type)
                {
                case BufferBindingDesc::CBV:
                case BufferBindingDesc::SRV:
                    access.reads.insert(key);
                    break;
                case BufferBindingDesc::UAV:
                    access.reads.insert(key);
                    access.writes.insert(key);
                    break;
                }
            }

            for (const auto& texture_pair : desc.draw_info.texture_resources)
            {
                const auto& binding = texture_pair.second;
                for (const auto texture_handle : binding.textures)
                {
                    ResourceKey key{ResourceKind::Texture, static_cast<unsigned long long>(texture_handle.value)};
                    if (binding.type == TextureBindingDesc::SRV)
                    {
                        access.reads.insert(key);
                    }
                    else
                    {
                        access.reads.insert(key);
                        access.writes.insert(key);
                    }
                }
            }

            for (const auto& render_target_pair : desc.draw_info.render_target_texture_resources)
            {
                const auto& binding = render_target_pair.second;
                for (const auto render_target_handle : binding.render_target_texture)
                {
                    ResourceKey key{ResourceKind::RenderTarget, static_cast<unsigned long long>(render_target_handle.value)};
                    if (binding.type == RenderTargetTextureBindingDesc::SRV)
                    {
                        access.reads.insert(key);
                    }
                    else
                    {
                        access.reads.insert(key);
                        access.writes.insert(key);
                    }
                }
            }

            for (const auto& render_target_pair : desc.draw_info.render_target_resources)
            {
                const auto& binding = render_target_pair.second;
                ResourceKey key{ResourceKind::RenderTarget, static_cast<unsigned long long>(render_target_pair.first.value)};
                access.writes.insert(key);
                if (binding.load_op == RenderPassAttachmentLoadOp::LOAD)
                {
                    access.reads.insert(key);
                }
            }

            return access;
        }

        FrameResourceAccessDiagnosticsData CollectFrameResourceAccessDiagnostics(
            const std::vector<RenderGraphNodeHandle>& nodes,
            const std::vector<RenderGraphNodeDesc>& render_graph_nodes)
        {
            FrameResourceAccessDiagnosticsData diagnostics_data{};
            for (const auto node_handle : nodes)
            {
                const auto& node_desc = render_graph_nodes[node_handle.value];
                const std::string group_name = node_desc.debug_group.empty() ? "<group-empty>" : node_desc.debug_group;
                const std::string pass_name = node_desc.debug_name.empty() ? "<pass-empty>" : node_desc.debug_name;
                const std::string pass_label = group_name + "/" + pass_name;

                const auto add_read = [&](ResourceKind kind, unsigned long long value)
                {
                    const ResourceKey resource_key{kind, value};
                    const unsigned long long encoded_key = EncodeResourceKey(resource_key);
                    diagnostics_data.access_masks[encoded_key] |= RESOURCE_ACCESS_MASK_READ;
                    AppendUniquePassName(diagnostics_data.pass_accesses[encoded_key].first, pass_label);
                };

                const auto add_write = [&](ResourceKind kind, unsigned long long value)
                {
                    const ResourceKey resource_key{kind, value};
                    const unsigned long long encoded_key = EncodeResourceKey(resource_key);
                    diagnostics_data.access_masks[encoded_key] |= RESOURCE_ACCESS_MASK_WRITE;
                    AppendUniquePassName(diagnostics_data.pass_accesses[encoded_key].second, pass_label);
                };

                for (const auto& buffer_pair : node_desc.draw_info.buffer_resources)
                {
                    const auto& binding = buffer_pair.second;
                    const unsigned long long resource_value = static_cast<unsigned long long>(binding.buffer_handle.value);
                    switch (binding.binding_type)
                    {
                    case BufferBindingDesc::CBV:
                    case BufferBindingDesc::SRV:
                        add_read(ResourceKind::Buffer, resource_value);
                        break;
                    case BufferBindingDesc::UAV:
                        add_read(ResourceKind::Buffer, resource_value);
                        add_write(ResourceKind::Buffer, resource_value);
                        break;
                    }
                }

                for (const auto& texture_pair : node_desc.draw_info.texture_resources)
                {
                    const auto& binding = texture_pair.second;
                    for (const auto texture_handle : binding.textures)
                    {
                        const unsigned long long resource_value = static_cast<unsigned long long>(texture_handle.value);
                        if (binding.type == TextureBindingDesc::SRV)
                        {
                            add_read(ResourceKind::Texture, resource_value);
                        }
                        else
                        {
                            add_read(ResourceKind::Texture, resource_value);
                            add_write(ResourceKind::Texture, resource_value);
                        }
                    }
                }

                for (const auto& render_target_pair : node_desc.draw_info.render_target_texture_resources)
                {
                    const auto& binding = render_target_pair.second;
                    for (const auto render_target_handle : binding.render_target_texture)
                    {
                        const unsigned long long resource_value = ResolveRenderTargetResourceIdentity(render_target_handle);
                        if (binding.type == RenderTargetTextureBindingDesc::SRV)
                        {
                            add_read(ResourceKind::RenderTarget, resource_value);
                        }
                        else
                        {
                            add_read(ResourceKind::RenderTarget, resource_value);
                            add_write(ResourceKind::RenderTarget, resource_value);
                        }
                    }
                }

                for (const auto& render_target_pair : node_desc.draw_info.render_target_resources)
                {
                    const auto& binding = render_target_pair.second;
                    const unsigned long long resource_value = ResolveRenderTargetResourceIdentity(render_target_pair.first);
                    add_write(ResourceKind::RenderTarget, resource_value);
                    if (binding.load_op == RenderPassAttachmentLoadOp::LOAD)
                    {
                        add_read(ResourceKind::RenderTarget, resource_value);
                    }
                }
            }

            return diagnostics_data;
        }

        void CollectResourceReadersAndWriters(
            const std::vector<RenderGraphNodeHandle>& nodes,
            const std::vector<RenderGraphNodeDesc>& render_graph_nodes,
            std::map<ResourceKey, std::set<RenderGraphNodeHandle>>& out_readers,
            std::map<ResourceKey, std::set<RenderGraphNodeHandle>>& out_writers)
        {
            out_readers.clear();
            out_writers.clear();

            for (auto handle : nodes)
            {
                const auto& node_desc = render_graph_nodes[handle.value];
                const auto access = CollectResourceAccess(node_desc);
                for (const auto& resource : access.reads)
                {
                    out_readers[resource].insert(handle);
                }
                for (const auto& resource : access.writes)
                {
                    out_writers[resource].insert(handle);
                }
            }
        }

        void BuildResourceInferredEdges(
            const std::map<ResourceKey, std::set<RenderGraphNodeHandle>>& resource_readers,
            const std::map<ResourceKey, std::set<RenderGraphNodeHandle>>& resource_writers,
            DependencyEdgeMap& out_edges,
            DependencyEdgeResourceMap* out_edge_resources = nullptr)
        {
            out_edges.clear();
            if (out_edge_resources)
            {
                out_edge_resources->clear();
            }

            for (const auto& writer_pair : resource_writers)
            {
                const auto& resource = writer_pair.first;
                const auto& writers = writer_pair.second;
                auto readers_it = resource_readers.find(resource);
                if (readers_it == resource_readers.end())
                {
                    continue;
                }
                const auto& readers = readers_it->second;

                for (const auto writer : writers)
                {
                    for (const auto reader : readers)
                    {
                        if (writer == reader)
                        {
                            continue;
                        }

                        out_edges[writer].insert(reader);
                        if (out_edge_resources)
                        {
                            (*out_edge_resources)[{writer, reader}].push_back(resource);
                        }
                    }
                }
            }
        }

        std::size_t ComputeExecutionSignature(
            const std::vector<RenderGraphNodeHandle>& nodes,
            const std::vector<RenderGraphNodeDesc>& render_graph_nodes,
            const DependencyEdgeMap& edges)
        {
            std::size_t signature = 1469598103934665603ULL;
            for (auto handle : nodes)
            {
                HashCombine(signature, handle.value);
                const auto& node_desc = render_graph_nodes[handle.value];
                const auto access = CollectResourceAccess(node_desc);

                HashCombine(signature, 0xA11u);
                for (const auto& resource : access.reads)
                {
                    HashCombine(signature, static_cast<std::size_t>(resource.kind));
                    HashCombine(signature, resource.value);
                }

                HashCombine(signature, 0xB22u);
                for (const auto& resource : access.writes)
                {
                    HashCombine(signature, static_cast<std::size_t>(resource.kind));
                    HashCombine(signature, resource.value);
                }

                HashCombine(signature, 0xC33u);
                for (const auto dep : node_desc.dependency_render_graph_nodes)
                {
                    HashCombine(signature, dep.value);
                }

                HashCombine(signature, 0xD44u);
                auto edges_it = edges.find(handle);
                if (edges_it != edges.end())
                {
                    for (const auto to : edges_it->second)
                    {
                        HashCombine(signature, to.value);
                    }
                }
            }

            return signature;
        }

        bool TopologicalSortExecutionNodes(
            const std::vector<RenderGraphNodeHandle>& nodes,
            const DependencyEdgeMap& edges,
            std::vector<RenderGraphNodeHandle>& out_execution_nodes,
            std::vector<RenderGraphNodeHandle>* out_cycle_nodes = nullptr)
        {
            out_execution_nodes.clear();
            if (out_cycle_nodes)
            {
                out_cycle_nodes->clear();
            }

            std::map<RenderGraphNodeHandle, unsigned> indegree;
            for (auto handle : nodes)
            {
                indegree[handle] = 0;
            }
            for (const auto& edge_pair : edges)
            {
                for (const auto& to : edge_pair.second)
                {
                    auto indegree_it = indegree.find(to);
                    if (indegree_it != indegree.end())
                    {
                        ++indegree_it->second;
                    }
                }
            }

            std::set<RenderGraphNodeHandle> ready;
            for (const auto& count : indegree)
            {
                if (count.second == 0)
                {
                    ready.insert(count.first);
                }
            }

            while (!ready.empty())
            {
                const auto node = *ready.begin();
                ready.erase(ready.begin());
                out_execution_nodes.push_back(node);

                auto edges_it = edges.find(node);
                if (edges_it == edges.end())
                {
                    continue;
                }
                for (const auto& to : edges_it->second)
                {
                    auto indegree_it = indegree.find(to);
                    if (indegree_it == indegree.end())
                    {
                        continue;
                    }

                    auto& count = indegree_it->second;
                    if (count > 0)
                    {
                        --count;
                        if (count == 0)
                        {
                            ready.insert(to);
                        }
                    }
                }
            }

            const bool sorted = out_execution_nodes.size() == nodes.size();
            if (!sorted && out_cycle_nodes)
            {
                for (const auto& count : indegree)
                {
                    if (count.second > 0)
                    {
                        out_cycle_nodes->push_back(count.first);
                    }
                }
            }
            return sorted;
        }

        bool ValidateExecutionOrder(
            const std::vector<RenderGraphNodeHandle>& nodes,
            const DependencyEdgeMap& edges,
            const std::vector<RenderGraphNodeHandle>& execution_order)
        {
            if (execution_order.size() != nodes.size())
            {
                return false;
            }

            std::set<RenderGraphNodeHandle> node_set(nodes.begin(), nodes.end());
            std::map<RenderGraphNodeHandle, std::size_t> order_index;
            for (std::size_t index = 0; index < execution_order.size(); ++index)
            {
                const auto node = execution_order[index];
                if (!node.IsValid() || !node_set.contains(node) || order_index.contains(node))
                {
                    return false;
                }
                order_index[node] = index;
            }

            if (order_index.size() != node_set.size())
            {
                return false;
            }

            for (const auto& edge_pair : edges)
            {
                const auto from = edge_pair.first;
                if (!node_set.contains(from))
                {
                    continue;
                }

                auto from_it = order_index.find(from);
                if (from_it == order_index.end())
                {
                    return false;
                }

                for (const auto to : edge_pair.second)
                {
                    if (!node_set.contains(to))
                    {
                        continue;
                    }

                    auto to_it = order_index.find(to);
                    if (to_it == order_index.end())
                    {
                        return false;
                    }

                    if (from_it->second >= to_it->second)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        struct ExecutionPlanBuildResult
        {
            DependencyEdgeMap combined_edges;
            bool has_invalid_explicit_dependency{false};
            std::vector<std::pair<RenderGraphNodeHandle, RenderGraphNodeHandle>> invalid_explicit_dependencies;
            std::size_t auto_merged_dependency_count{0};
            std::size_t signature{0};
            bool execution_cache_key_changed{false};
            bool cached_execution_order_valid{false};
            bool graph_just_became_invalid{false};
            bool need_rebuild_execution_order{false};
        };

        struct ResourceAccessPlanResult
        {
            DependencyEdgeMap inferred_edges;
            std::size_t auto_merged_dependency_count{0};
        };

        ResourceAccessPlanResult CollectResourceAccessPlan(const RenderGraph::ExecutionPlanContext& context)
        {
            std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_readers;
            std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_writers;
            CollectResourceReadersAndWriters(context.nodes, context.render_graph_nodes, resource_readers, resource_writers);

            ResourceAccessPlanResult result{};
            BuildResourceInferredEdges(resource_readers, resource_writers, result.inferred_edges, nullptr);

            for (const auto& edge_pair : result.inferred_edges)
            {
                const auto from = edge_pair.first;
                for (const auto to : edge_pair.second)
                {
                    if (!to.IsValid() || to.value >= context.render_graph_nodes.size())
                    {
                        continue;
                    }

                    const auto& to_desc = context.render_graph_nodes[to.value];
                    if (!HasDependency(to_desc, from))
                    {
                        ++result.auto_merged_dependency_count;
                    }
                }
            }

            return result;
        }

        struct DependencyValidationResult
        {
            DependencyEdgeMap combined_edges;
            bool has_invalid_explicit_dependency{false};
            std::vector<std::pair<RenderGraphNodeHandle, RenderGraphNodeHandle>> invalid_explicit_dependencies;
        };

        DependencyValidationResult ValidateDependencyPlan(
            const RenderGraph::ExecutionPlanContext& context,
            const DependencyEdgeMap& inferred_edges)
        {
            DependencyValidationResult result{};
            result.combined_edges = inferred_edges;

            for (auto handle : context.nodes)
            {
                const auto& node_desc = context.render_graph_nodes[handle.value];
                for (const auto dep : node_desc.dependency_render_graph_nodes)
                {
                    const bool valid_dep = dep.IsValid() &&
                        dep != handle &&
                        dep.value < context.render_graph_nodes.size() &&
                        context.registered_nodes.contains(dep);
                    if (!valid_dep)
                    {
                        result.has_invalid_explicit_dependency = true;
                        result.invalid_explicit_dependencies.emplace_back(dep, handle);
                        continue;
                    }
                    result.combined_edges[dep].insert(handle);
                }
            }

            return result;
        }

        ExecutionPlanBuildResult BuildExecutionPlan(const RenderGraph::ExecutionPlanContext& context)
        {
            const auto access_plan = CollectResourceAccessPlan(context);
            const auto dependency_validation = ValidateDependencyPlan(context, access_plan.inferred_edges);

            ExecutionPlanBuildResult result{};
            result.combined_edges = dependency_validation.combined_edges;
            result.has_invalid_explicit_dependency = dependency_validation.has_invalid_explicit_dependency;
            result.invalid_explicit_dependencies = dependency_validation.invalid_explicit_dependencies;
            result.auto_merged_dependency_count = access_plan.auto_merged_dependency_count;

            result.signature = ComputeExecutionSignature(context.nodes, context.render_graph_nodes, result.combined_edges);
            result.execution_cache_key_changed =
                result.signature != context.cached_execution_signature || context.cached_execution_node_count != context.nodes.size();
            result.cached_execution_order_valid = ValidateExecutionOrder(context.nodes, result.combined_edges, context.cached_execution_order);
            result.graph_just_became_invalid = result.has_invalid_explicit_dependency && context.cached_execution_graph_valid;
            result.need_rebuild_execution_order =
                result.execution_cache_key_changed ||
                result.graph_just_became_invalid ||
                (context.cached_execution_graph_valid && !result.cached_execution_order_valid);

            return result;
        }

        std::vector<RenderGraphNodeHandle> ApplyExecutionPlanResult(
            const RenderGraph::ExecutionPlanContext& context,
            const ExecutionPlanBuildResult& plan,
            RenderGraph::ExecutionPlanCacheState& io_cache_state)
        {
            std::vector<RenderGraphNodeHandle> rebuilt_cycle_nodes;
            if (plan.need_rebuild_execution_order)
            {
                if (!plan.execution_cache_key_changed && !plan.graph_just_became_invalid && !plan.cached_execution_order_valid)
                {
                    LOG_FORMAT_FLUSH("[RenderGraph][Dependency] Cached execution order validation failed. Rebuilding execution order.\n");
                }

                io_cache_state.cached_execution_signature = plan.signature;
                io_cache_state.cached_execution_node_count = context.nodes.size();

                {
                    static bool s_has_auto_merge_log_state = false;
                    static std::size_t s_last_auto_merged_dependency_count = 0;
                    static std::chrono::steady_clock::time_point s_last_auto_merge_log_time{};
                    if (plan.auto_merged_dependency_count > 0)
                    {
                        const auto now = std::chrono::steady_clock::now();
                        constexpr auto auto_merge_log_min_interval = std::chrono::seconds(5);
                        const bool count_changed =
                            !s_has_auto_merge_log_state ||
                            s_last_auto_merged_dependency_count != plan.auto_merged_dependency_count;
                        const bool interval_elapsed =
                            !s_has_auto_merge_log_state ||
                            (now - s_last_auto_merge_log_time) >= auto_merge_log_min_interval;
                        if (count_changed || interval_elapsed)
                        {
                            LOG_FORMAT_FLUSH("[RenderGraph][Dependency] Auto-merged %zu inferred dependencies into execution plan.\n",
                                plan.auto_merged_dependency_count);
                            s_last_auto_merge_log_time = now;
                        }
                    }

                    s_has_auto_merge_log_state = true;
                    s_last_auto_merged_dependency_count = plan.auto_merged_dependency_count;
                }

                if (plan.has_invalid_explicit_dependency)
                {
                    io_cache_state.cached_execution_graph_valid = false;
                    io_cache_state.cached_execution_order.clear();
                    LOG_FORMAT_FLUSH("[RenderGraph][Dependency] Invalid explicit dependencies detected (%zu). Skip frame graph execution.\n",
                        plan.invalid_explicit_dependencies.size());
                    for (const auto& invalid_dep : plan.invalid_explicit_dependencies)
                    {
                        const auto from = invalid_dep.first;
                        const auto to = invalid_dep.second;
                        if (from.IsValid())
                        {
                            LOG_FORMAT_FLUSH("[RenderGraph][Dependency]   invalid edge: %u -> %u\n", from.value, to.value);
                        }
                        else
                        {
                            LOG_FORMAT_FLUSH("[RenderGraph][Dependency]   invalid edge: <INVALID> -> %u\n", to.value);
                        }
                    }
                }
                else
                {
                    std::vector<RenderGraphNodeHandle> execution_nodes;
                    std::vector<RenderGraphNodeHandle> cycle_nodes;
                    const bool sorted = TopologicalSortExecutionNodes(context.nodes, plan.combined_edges, execution_nodes, &cycle_nodes);
                    if (!sorted)
                    {
                        io_cache_state.cached_execution_graph_valid = false;
                        io_cache_state.cached_execution_order.clear();
                        rebuilt_cycle_nodes = cycle_nodes;
                        LOG_FORMAT_FLUSH("[RenderGraph][Dependency] Cycle detected in combined dependencies. Skip frame graph execution. Nodes:");
                        for (const auto cycle_node : cycle_nodes)
                        {
                            LOG_FORMAT_FLUSH(" %u", cycle_node.value);
                        }
                        LOG_FORMAT_FLUSH("\n");
                        for (const auto cycle_node : cycle_nodes)
                        {
                            const auto& node_desc = context.render_graph_nodes[cycle_node.value];
                            const char* group_name = node_desc.debug_group.empty() ? "<group-empty>" : node_desc.debug_group.c_str();
                            const char* pass_name = node_desc.debug_name.empty() ? "<pass-empty>" : node_desc.debug_name.c_str();
                            LOG_FORMAT_FLUSH("[RenderGraph][Dependency]   cycle node %u = (%s/%s)\n",
                                             cycle_node.value,
                                             group_name,
                                             pass_name);
                        }

                        std::set<RenderGraphNodeHandle> cycle_node_set(cycle_nodes.begin(), cycle_nodes.end());
                        std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_readers;
                        std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_writers;
                        CollectResourceReadersAndWriters(context.nodes, context.render_graph_nodes, resource_readers, resource_writers);
                        DependencyEdgeMap inferred_edges_for_debug;
                        DependencyEdgeResourceMap inferred_edge_resources_for_debug;
                        BuildResourceInferredEdges(
                            resource_readers,
                            resource_writers,
                            inferred_edges_for_debug,
                            &inferred_edge_resources_for_debug);

                        const auto log_resource_key = [](const ResourceKey& key)
                        {
                            LOG_FORMAT_FLUSH(" %s:%llu", ToResourceKindName(key.kind), key.value);
                        };

                        for (const auto cycle_from : cycle_nodes)
                        {
                            auto combined_edge_it = plan.combined_edges.find(cycle_from);
                            if (combined_edge_it == plan.combined_edges.end())
                            {
                                continue;
                            }

                            for (const auto cycle_to : combined_edge_it->second)
                            {
                                if (!cycle_node_set.contains(cycle_to))
                                {
                                    continue;
                                }

                                LOG_FORMAT_FLUSH("[RenderGraph][Dependency]   cycle edge %u -> %u", cycle_from.value, cycle_to.value);
                                const auto resource_it = inferred_edge_resources_for_debug.find({cycle_from, cycle_to});
                                if (resource_it == inferred_edge_resources_for_debug.end())
                                {
                                    LOG_FORMAT_FLUSH(" (explicit)\n");
                                    continue;
                                }

                                const auto& resources = resource_it->second;
                                LOG_FORMAT_FLUSH(" (inferred resources:");
                                const size_t max_resource_print_count = (std::min)(resources.size(), static_cast<size_t>(8));
                                for (size_t resource_index = 0; resource_index < max_resource_print_count; ++resource_index)
                                {
                                    log_resource_key(resources[resource_index]);
                                }
                                if (resources.size() > max_resource_print_count)
                                {
                                    LOG_FORMAT_FLUSH(" ...");
                                }
                                LOG_FORMAT_FLUSH(")\n");
                            }
                        }
                    }
                    else
                    {
                        io_cache_state.cached_execution_graph_valid = true;
                        io_cache_state.cached_execution_order = std::move(execution_nodes);
                        rebuilt_cycle_nodes.clear();
                    }
                }
            }

            if (plan.has_invalid_explicit_dependency || io_cache_state.cached_execution_graph_valid)
            {
                return {};
            }
            if (plan.need_rebuild_execution_order)
            {
                return rebuilt_cycle_nodes;
            }

            return {};
        }

        void UpdateDependencyDiagnostics(
            const RenderGraph::ExecutionPlanContext& context,
            const ExecutionPlanBuildResult& plan,
            bool cached_execution_graph_valid,
            std::size_t cached_execution_signature,
            std::size_t cached_execution_node_count,
            std::size_t cached_execution_order_size,
            std::vector<RenderGraphNodeHandle>&& cycle_nodes,
            const std::map<RenderGraphNodeHandle, std::tuple<unsigned, unsigned, unsigned>>& auto_pruned_named_binding_counts,
            const ResourceAccessMaskMap& previous_frame_resource_access_masks,
            const ResourcePassAccessMap& previous_frame_resource_pass_accesses,
            bool has_previous_frame_resource_access_masks,
            unsigned cross_frame_comparison_window_size,
            unsigned cross_frame_compared_frame_count,
            const ResourceAccessMaskMap& current_frame_resource_access_masks,
            const ResourcePassAccessMap& current_frame_resource_pass_accesses,
            RenderGraph::DependencyDiagnostics& out_diagnostics)
        {
            out_diagnostics.graph_valid = cached_execution_graph_valid;
            out_diagnostics.has_invalid_explicit_dependencies = plan.has_invalid_explicit_dependency;
            out_diagnostics.auto_merged_dependency_count = static_cast<unsigned>(plan.auto_merged_dependency_count);
            out_diagnostics.invalid_explicit_dependencies = plan.invalid_explicit_dependencies;
            out_diagnostics.execution_signature = cached_execution_signature;
            out_diagnostics.cached_execution_node_count = cached_execution_node_count;
            out_diagnostics.cached_execution_order_size = cached_execution_order_size;
            if (plan.has_invalid_explicit_dependency || cached_execution_graph_valid)
            {
                out_diagnostics.cycle_nodes.clear();
            }
            else if (plan.need_rebuild_execution_order)
            {
                out_diagnostics.cycle_nodes = std::move(cycle_nodes);
            }

            out_diagnostics.auto_pruned_binding_count = 0;
            out_diagnostics.auto_pruned_node_count = 0;
            out_diagnostics.auto_pruned_nodes.clear();
            for (const auto node_handle : context.nodes)
            {
                const auto pruned_count_it = auto_pruned_named_binding_counts.find(node_handle);
                if (pruned_count_it == auto_pruned_named_binding_counts.end())
                {
                    continue;
                }

                const auto [buffer_count, texture_count, render_target_texture_count] = pruned_count_it->second;
                const unsigned total_count = buffer_count + texture_count + render_target_texture_count;
                if (total_count == 0)
                {
                    continue;
                }

                ++out_diagnostics.auto_pruned_node_count;
                out_diagnostics.auto_pruned_binding_count += total_count;

                RenderGraph::DependencyDiagnostics::AutoPrunedNodeDiagnostics node_diagnostics{};
                node_diagnostics.node_handle = node_handle;
                const auto& node_desc = context.render_graph_nodes[node_handle.value];
                node_diagnostics.group_name = node_desc.debug_group.empty() ? "<group-empty>" : node_desc.debug_group;
                node_diagnostics.pass_name = node_desc.debug_name.empty() ? "<pass-empty>" : node_desc.debug_name;
                node_diagnostics.buffer_count = buffer_count;
                node_diagnostics.texture_count = texture_count;
                node_diagnostics.render_target_texture_count = render_target_texture_count;
                out_diagnostics.auto_pruned_nodes.push_back(std::move(node_diagnostics));
            }

            out_diagnostics.cross_frame_analysis_ready = has_previous_frame_resource_access_masks;
            out_diagnostics.cross_frame_comparison_window_size = cross_frame_comparison_window_size;
            out_diagnostics.cross_frame_compared_frame_count = cross_frame_compared_frame_count;
            out_diagnostics.cross_frame_hazard_count = 0;
            out_diagnostics.cross_frame_hazard_overflow_count = 0;
            out_diagnostics.cross_frame_hazards.clear();
            if (!has_previous_frame_resource_access_masks)
            {
                return;
            }

            const auto record_hazard = [
                &out_diagnostics,
                &previous_frame_resource_pass_accesses,
                &current_frame_resource_pass_accesses
            ](
                unsigned long long encoded_resource_key,
                RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType hazard_type,
                bool use_previous_writers,
                bool use_current_writers)
            {
                ++out_diagnostics.cross_frame_hazard_count;
                if (out_diagnostics.cross_frame_hazards.size() >= MAX_CROSS_FRAME_HAZARDS_RECORDED)
                {
                    ++out_diagnostics.cross_frame_hazard_overflow_count;
                    return;
                }

                constexpr unsigned long long kind_shift = 62ull;
                const auto encoded_kind = static_cast<unsigned>(encoded_resource_key >> kind_shift);
                const auto resource_kind = static_cast<ResourceKind>(encoded_kind);
                RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics hazard{};
                hazard.hazard_type = hazard_type;
                hazard.resource_kind = ToResourceKindName(resource_kind);
                hazard.resource_id = static_cast<unsigned>(encoded_resource_key & 0xffffffffULL);

                const auto previous_access_it = previous_frame_resource_pass_accesses.find(encoded_resource_key);
                if (previous_access_it != previous_frame_resource_pass_accesses.end())
                {
                    hazard.previous_passes = use_previous_writers
                        ? previous_access_it->second.second
                        : previous_access_it->second.first;
                }

                const auto current_access_it = current_frame_resource_pass_accesses.find(encoded_resource_key);
                if (current_access_it != current_frame_resource_pass_accesses.end())
                {
                    hazard.current_passes = use_current_writers
                        ? current_access_it->second.second
                        : current_access_it->second.first;
                }

                out_diagnostics.cross_frame_hazards.push_back(std::move(hazard));
            };

            for (const auto& current_pair : current_frame_resource_access_masks)
            {
                const auto previous_it = previous_frame_resource_access_masks.find(current_pair.first);
                if (previous_it == previous_frame_resource_access_masks.end())
                {
                    continue;
                }

                const unsigned char previous_mask = previous_it->second;
                const unsigned char current_mask = current_pair.second;
                if ((previous_mask & RESOURCE_ACCESS_MASK_WRITE) != 0 && (current_mask & RESOURCE_ACCESS_MASK_READ) != 0)
                {
                    record_hazard(
                        current_pair.first,
                        RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType::PREVIOUS_WRITE_CURRENT_READ,
                        true,
                        false);
                }
                if ((previous_mask & RESOURCE_ACCESS_MASK_WRITE) != 0 && (current_mask & RESOURCE_ACCESS_MASK_WRITE) != 0)
                {
                    record_hazard(
                        current_pair.first,
                        RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType::PREVIOUS_WRITE_CURRENT_WRITE,
                        true,
                        true);
                }
                if ((previous_mask & RESOURCE_ACCESS_MASK_READ) != 0 && (current_mask & RESOURCE_ACCESS_MASK_WRITE) != 0)
                {
                    record_hazard(
                        current_pair.first,
                        RenderGraph::DependencyDiagnostics::CrossFrameResourceHazardDiagnostics::HazardType::PREVIOUS_READ_CURRENT_WRITE,
                        false,
                        true);
                }
            }
        }

        unsigned long long ComputeBufferDescriptorCacheKey(const BufferBindingDesc& binding_desc, std::uintptr_t buffer_identity_key)
        {
            std::size_t seed = 1469598103934665603ULL;
            HashCombine(seed, static_cast<std::size_t>(binding_desc.binding_type));
            HashCombine(seed, static_cast<std::size_t>(buffer_identity_key));
            HashCombine(seed, static_cast<std::size_t>(binding_desc.stride));
            HashCombine(seed, static_cast<std::size_t>(binding_desc.count));
            HashCombine(seed, binding_desc.is_structured_buffer ? 1u : 0u);
            HashCombine(seed, binding_desc.use_count_buffer ? 1u : 0u);
            HashCombine(seed, static_cast<std::size_t>(binding_desc.count_buffer_offset));
            return static_cast<unsigned long long>(seed);
        }

        unsigned long long ComputeTextureDescriptorCacheKey(unsigned binding_type, const std::vector<std::uintptr_t>& source_texture_identity_keys)
        {
            std::size_t seed = 1469598103934665603ULL;
            HashCombine(seed, static_cast<std::size_t>(binding_type));
            HashCombine(seed, source_texture_identity_keys.size());
            for (const auto source_texture_identity_key : source_texture_identity_keys)
            {
                HashCombine(seed, static_cast<std::size_t>(source_texture_identity_key));
            }
            return static_cast<unsigned long long>(seed);
        }

        std::vector<std::uintptr_t> BuildTextureSourceIdentityKeys(const TextureBindingDesc& binding_desc)
        {
            std::vector<std::uintptr_t> source_texture_identity_keys;
            source_texture_identity_keys.reserve(binding_desc.textures.size());
            for (const auto texture_handle : binding_desc.textures)
            {
                const auto texture_allocation = InternalResourceHandleTable::Instance().GetTexture(texture_handle);
                if (texture_allocation && texture_allocation->m_texture)
                {
                    source_texture_identity_keys.push_back(reinterpret_cast<std::uintptr_t>(texture_allocation->m_texture.get()));
                }
                else
                {
                    source_texture_identity_keys.push_back(static_cast<std::uintptr_t>(texture_handle.value));
                }
            }
            return source_texture_identity_keys;
        }

        std::vector<std::uintptr_t> BuildRenderTargetTextureSourceIdentityKeys(const RenderTargetTextureBindingDesc& binding_desc)
        {
            std::vector<std::uintptr_t> source_texture_identity_keys;
            source_texture_identity_keys.reserve(binding_desc.render_target_texture.size());
            for (const auto render_target_handle : binding_desc.render_target_texture)
            {
                source_texture_identity_keys.push_back(static_cast<std::uintptr_t>(ResolveRenderTargetResourceIdentity(render_target_handle)));
            }
            return source_texture_identity_keys;
        }

        RHIPrimitiveTopologyType ConvertToRHIPrimitiveTopology(PrimitiveTopology topology)
        {
            switch (topology)
            {
            case PrimitiveTopology::TRIANGLE_LIST:
                return RHIPrimitiveTopologyType::TRIANGLELIST;
            }

            GLTF_CHECK(false);
            return RHIPrimitiveTopologyType::TRIANGLELIST;
        }
    }

    RenderWindow::RenderWindow(const RenderWindowDesc& desc)
        : m_desc(desc)
        , m_handle(NULL_HANDLE)
        , m_hwnd(nullptr)
    {
        glTFWindow::Get().SetWidth(desc.width);
        glTFWindow::Get().SetHeight(desc.height);
        
        glTFWindow::Get().InitAndShowWindow();
        m_hwnd = glTFWindow::Get().GetHWND();
        m_handle = InternalResourceHandleTable::Instance().RegisterWindow(*this);
        
        m_input_device = std::make_shared<RendererInputDevice>();
        glTFWindow::Get().SetInputManager(m_input_device);

        // Register window callback
        GLTF_CHECK(glTFWindow::Get().RegisterCallbackEventNative());
    }

    RenderWindowHandle RenderWindow::GetHandle() const
    {
        return m_handle;
    }

    unsigned RenderWindow::GetWidth() const
    {
        return static_cast<unsigned>(glTFWindow::Get().GetWidth());
    }

    unsigned RenderWindow::GetHeight() const
    {
        return static_cast<unsigned>(glTFWindow::Get().GetHeight());
    }

    HWND RenderWindow::GetHWND() const
    {
        return glTFWindow::Get().GetHWND();
    }

    void RenderWindow::EnterWindowEventLoop()
    {
        glTFWindow::Get().UpdateWindow();
    }

    RenderWindow::WindowLoopTiming RenderWindow::GetLastLoopTiming() const
    {
        const auto loop_timing = glTFWindow::Get().GetLastLoopTiming();
        WindowLoopTiming timing{};
        timing.valid = loop_timing.valid;
        timing.frame_index = loop_timing.frame_index;
        timing.loop_total_ms = loop_timing.loop_total_ms;
        timing.loop_thread_cpu_ms = loop_timing.loop_thread_cpu_ms;
        timing.idle_wait_ms = loop_timing.idle_wait_ms;
        timing.tick_callback_ms = loop_timing.tick_callback_ms;
        timing.tick_callback_thread_cpu_ms = loop_timing.tick_callback_thread_cpu_ms;
        timing.poll_events_ms = loop_timing.poll_events_ms;
        timing.poll_events_thread_cpu_ms = loop_timing.poll_events_thread_cpu_ms;
        timing.non_tick_ms = loop_timing.non_tick_ms;
        return timing;
    }

    int RenderWindow::GetWindowRefreshRate() const
    {
        return glTFWindow::Get().GetWindowRefreshRate();
    }

    void RenderWindow::RequestClose()
    {
        glTFWindow::Get().RequestClose();
    }

    bool RenderWindow::IsCloseRequested() const
    {
        return glTFWindow::Get().IsCloseRequested();
    }

    void RenderWindow::RegisterTickCallback(const RenderWindowTickCallback& callback)
    {
        glTFWindow::Get().SetTickCallback(callback);
    }

    void RenderWindow::RegisterExitCallback(const RenderWindowExitCallback& callback)
    {
        glTFWindow::Get().SetExitCallback(callback);
    }

    const RendererInputDevice& RenderWindow::GetInputDeviceConst() const
    {
        return *m_input_device;
    }

    RendererInputDevice& RenderWindow::GetInputDevice()
    {
        return *m_input_device;
    }

    RendererSceneResourceManager::RendererSceneResourceManager(ResourceOperator& allocator,const RenderSceneDesc& desc)
        : m_allocator(allocator)
    {
        std::shared_ptr<RendererSceneGraph> scene_graph = std::make_shared<RendererSceneGraph>();
        m_render_scene_handle = InternalResourceHandleTable::Instance().RegisterRenderScene(scene_graph);
        
        glTFLoader loader;
        bool loaded = loader.LoadFile(desc.scene_file_name);
        GLTF_CHECK(loaded);
        
        bool added = scene_graph->InitializeRootNodeWithSceneFile_glTF(loader);
        GLTF_CHECK(added);

        struct SceneMeshDataOffsetInfo
        {
            unsigned start_face_index;
            unsigned material_index;
            unsigned start_vertex_index;
        };

        struct SceneMeshFaceInfo
        {
            unsigned vertex_index[3];
        };

        struct SceneMeshVertexInfo
        {
            float position[4];
            float normal[4];
            float tangent[4];
            float uv[4];
        };

        struct glTFMeshInstanceRenderResource
        {
            glm::mat4 m_instance_transform;
            unsigned m_mesh_render_resource;
            unsigned m_instance_material_id;
            bool m_normal_mapping;
        };

        std::vector<SceneMeshDataOffsetInfo> start_offset_infos;
        std::vector<SceneMeshVertexInfo> mesh_vertex_infos;
        std::vector<SceneMeshFaceInfo> mesh_face_vertex_infos;
        unsigned start_face_offset = 0;

        const auto& meshes = scene_graph->GetMeshes();
        for (const auto& mesh_pair : meshes)
        {
            const auto& mesh = *mesh_pair.second;

            const unsigned vertex_offset = static_cast<unsigned>(mesh_vertex_infos.size());

            SceneMeshDataOffsetInfo mesh_info =
            {
                .start_face_index = start_face_offset,
                // TODO: Material system 
                .material_index = 0,
                .start_vertex_index = vertex_offset,
            };
                
            start_offset_infos.push_back(mesh_info);
            const size_t face_count = mesh.GetIndexBuffer().index_count / 3;
            const auto& index_buffer = mesh.GetIndexBuffer();
            for (size_t i = 0; i < face_count; ++i)
            {
                mesh_face_vertex_infos.push_back(
                {
                    vertex_offset + index_buffer.GetIndexByOffset(3 * i),
                    vertex_offset + index_buffer.GetIndexByOffset(3 * i + 1),
                    vertex_offset + index_buffer.GetIndexByOffset(3 * i + 2),
                });
            }

            for (size_t i = 0; i < mesh.GetVertexBuffer().vertex_count; ++i)
            {
                SceneMeshVertexInfo vertex_info;
                size_t out_data_size = 0;
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_POSITION, i, &vertex_info.position, out_data_size);
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_NORMAL, i, &vertex_info.normal, out_data_size);
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TANGENT, i, &vertex_info.tangent, out_data_size);
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TEXCOORD0, i, &vertex_info.uv, out_data_size);
                mesh_vertex_infos.push_back(vertex_info);
            }
        
            start_face_offset += face_count;
        }
    }

    bool RendererSceneResourceManager::AccessSceneData(RendererSceneMeshDataAccessorBase& data_accessor)
    {
        const auto& scene_graph = InternalResourceHandleTable::Instance().GetRenderScene(m_render_scene_handle);
        auto scene_node_traverse = [&](RendererSceneNode& node)
        {
            if (node.HasMesh())
            {
                auto absolute_transform = node.GetAbsoluteTransform();
                
                for (const auto& mesh : node.GetMeshes())
                {
                    auto mesh_id = mesh->GetID();
                    
                    if (!data_accessor.HasMeshData(mesh_id))
                    {
                        auto vertex_count = mesh->GetVertexBuffer().vertex_count;
                        std::vector<float> vertex_positions(vertex_count * 3);
                        std::vector<float> vertex_normals(vertex_count * 3);
                        std::vector<float> vertex_tangents(vertex_count * 4);
                        std::vector<float> vertex_uvs(vertex_count * 2);

                        for (size_t i = 0; i < vertex_count; ++i)
                        {
                            size_t out_data_size = 0;
                            mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_POSITION, i, &vertex_positions[3 * i], out_data_size);
                            GLTF_CHECK(out_data_size == 3 * sizeof(float));
                        
                            if (mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_NORMAL, i, &vertex_normals[3 * i], out_data_size))
                            {
                                GLTF_CHECK(out_data_size == 3 * sizeof(float));    
                            }
                            else
                            {
                                LOG_FORMAT_FLUSH("[WARN] Mesh %d has no normal vertex data!\n", mesh->GetID());
                            }

                            if (mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TANGENT, i, &vertex_tangents[4 * i], out_data_size))
                            {
                                GLTF_CHECK(out_data_size == 4 * sizeof(float));    
                            }
                            else
                            {
                                LOG_FORMAT_FLUSH("[WARN] Mesh %d has no tangent vertex data!\n", mesh->GetID());
                            }
                            
                            if (mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TEXCOORD0, i, &vertex_uvs[2 * i], out_data_size))
                            {
                                GLTF_CHECK(out_data_size == 2 * sizeof(float));    
                            }
                            else
                            {
                                LOG_FORMAT_FLUSH("[WARN] Mesh %d has no uv vertex data!\n", mesh->GetID());
                            }
                        }

                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_POSITION_FLOAT3, mesh_id, vertex_positions.data(), vertex_count);
                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_NORMAL_FLOAT3, mesh_id, vertex_normals.data(), vertex_count);
                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_TANGENT_FLOAT4, mesh_id, vertex_tangents.data(), vertex_count);
                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_TEXCOORD0_FLOAT2, mesh_id, vertex_uvs.data(), vertex_count);

                        auto index = mesh->GetIndexBuffer().data.get();
                        auto index_count = mesh->GetIndexBuffer().index_count;
                        data_accessor.AccessMeshData(mesh->GetIndexBuffer().format == RHIDataFormat::R16_UINT ? RendererSceneMeshDataAccessorBase::MeshDataAccessorType::INDEX_HALF : RendererSceneMeshDataAccessorBase::MeshDataAccessorType::INDEX_INT, mesh_id, index, index_count);

                        data_accessor.AccessMaterialData(mesh->GetMaterial(), mesh_id);
                    }
                    
                    data_accessor.AccessInstanceData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::INSTANCE_MAT4x4, node.GetID(), mesh_id, &absolute_transform, 1);
                }
            }
            
            // No stop
            return false;
        };
        
        scene_graph->GetRootNode().Traverse(scene_node_traverse);
        
        return true;
    }

    RendererSceneAABB RendererSceneResourceManager::GetSceneBounds() const
    {
        const auto& scene_graph = InternalResourceHandleTable::Instance().GetRenderScene(m_render_scene_handle);
        GLTF_CHECK(scene_graph);
        return scene_graph->GetBounds();
    }

    ResourceOperator::ResourceOperator(RenderDeviceDesc device)
    {
        if (!m_resource_manager)
        {
            RHIConfigSingleton::Instance().SetGraphicsAPIType(device.type == VULKAN ?
            RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan : RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12);
    
            RHIConfigSingleton::Instance().InitGraphicsAPI();
            
            m_resource_manager = std::make_shared<ResourceManager>();
            m_resource_manager->InitResourceManager(device);
        }
    }

    ShaderHandle ResourceOperator::CreateShader(const ShaderDesc& desc)
    {
        return m_resource_manager->CreateShader(desc);
    }

    TextureHandle ResourceOperator::CreateTexture(const TextureDesc& desc)
    {
        const RHIDataFormat rhi_format = RendererInterfaceRHIConverter::ConvertToRHIFormat(desc.format);
        GLTF_CHECK(rhi_format != RHIDataFormat::UNKNOWN);

        RHITextureClearValue clear_value{};
        clear_value.clear_format = rhi_format;
        clear_value.clear_color[0] = 0.0f;
        clear_value.clear_color[1] = 0.0f;
        clear_value.clear_color[2] = 0.0f;
        clear_value.clear_color[3] = 0.0f;

        RHITextureDesc texture_desc(
            "runtime_texture",
            desc.width,
            desc.height,
            rhi_format,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST | (desc.generate_mipmaps ? RUF_CONTAINS_MIPMAP : RUF_NONE)),
            clear_value);

        std::shared_ptr<IRHITextureAllocation> texture_allocation = nullptr;
        bool created = false;
        if (!desc.data.empty())
        {
            const bool has_data = texture_desc.SetTextureData(desc.data.data(), desc.data.size());
            GLTF_CHECK(has_data);
            created = m_resource_manager->GetMemoryManager().AllocateTextureMemoryAndUpload(
                m_resource_manager->GetDevice(),
                m_resource_manager->GetCommandListForRecordPassCommand(),
                m_resource_manager->GetMemoryManager(),
                texture_desc,
                texture_allocation);
        }
        else
        {
            created = m_resource_manager->GetMemoryManager().AllocateTextureMemory(
                m_resource_manager->GetDevice(),
                texture_desc,
                texture_allocation);
        }
        GLTF_CHECK(created);

        texture_allocation->m_texture->Transition(
            m_resource_manager->GetCommandListForRecordPassCommand(),
            RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);

        return InternalResourceHandleTable::Instance().RegisterTexture(texture_allocation);
    }

    TextureHandle ResourceOperator::CreateTexture(const TextureFileDesc& desc)
    {
        const std::wstring convert_path = to_wide_string(desc.uri);
        ImageLoadResult result;
        if (!glTFImageIOUtil::Instance().LoadImageByFilename(convert_path.c_str(), result))
        {
            GLTF_CHECK(false);
            return NULL_HANDLE;
        }
        RHITextureDesc texture_desc{};
        texture_desc.InitWithLoadedData(result);

        std::shared_ptr<IRHITextureAllocation> texture_allocation = nullptr;
        bool created = m_resource_manager->GetMemoryManager().AllocateTextureMemoryAndUpload(m_resource_manager->GetDevice(), m_resource_manager->GetCommandListForRecordPassCommand(), m_resource_manager->GetMemoryManager(), texture_desc, texture_allocation);
        GLTF_CHECK(created);

        texture_allocation->m_texture->Transition(m_resource_manager->GetCommandListForRecordPassCommand(), RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);

        return InternalResourceHandleTable::Instance().RegisterTexture(texture_allocation);
    }

    BufferHandle ResourceOperator::CreateBuffer(const BufferDesc& desc)
    {
        return m_resource_manager->CreateBuffer(desc);
    }

    bool ResourceOperator::RetireBuffer(BufferHandle handle)
    {
        if (!m_resource_manager || !handle.IsValid())
        {
            return false;
        }

        return m_resource_manager->RetireBuffer(handle);
    }

    IndexedBufferHandle ResourceOperator::CreateIndexedBuffer(const BufferDesc& desc)
    {
        return m_resource_manager->CreateIndexedBuffer(desc);   
    }

    std::vector<BufferHandle> ResourceOperator::CreateFrameBufferedBuffers(const BufferDesc& desc, const std::string& debug_name_prefix)
    {
        const auto frame_context = GetFrameContext();
        const unsigned frame_slot_count = frame_context.frame_slot_count;
        std::vector<BufferHandle> buffers;
        buffers.reserve(frame_slot_count);

        std::string base_name = debug_name_prefix;
        if (base_name.empty())
        {
            base_name = desc.name.empty() ? "FrameBufferedBuffer" : desc.name;
        }

        for (unsigned frame_index = 0; frame_index < frame_slot_count; ++frame_index)
        {
            BufferDesc frame_desc = desc;
            frame_desc.name = base_name + "_frame_" + std::to_string(frame_index);
            buffers.push_back(CreateBuffer(frame_desc));
        }

        return buffers;
    }

    bool ResourceOperator::RetireFrameBufferedBuffers(const std::vector<BufferHandle>& buffers)
    {
        bool retired_all = !buffers.empty();
        std::set<BufferHandle> unique_handles;
        for (const auto handle : buffers)
        {
            if (!handle.IsValid() || !unique_handles.insert(handle).second)
            {
                continue;
            }

            retired_all = RetireBuffer(handle) && retired_all;
        }

        return retired_all;
    }

    BufferHandle ResourceOperator::GetFrameBufferedBufferHandle(const std::vector<BufferHandle>& buffers) const
    {
        return GetFrameBufferedBufferHandle(buffers, GetFrameContext());
    }

    BufferHandle ResourceOperator::GetFrameBufferedBufferHandle(const std::vector<BufferHandle>& buffers, const FrameContextSnapshot& frame_context) const
    {
        GLTF_CHECK(!buffers.empty());
        const unsigned frame_slot = frame_context.per_frame_resource_binding_enabled
            ? (frame_context.frame_slot_index % static_cast<unsigned>(buffers.size()))
            : 0u;
        return buffers[frame_slot];
    }

    void ResourceOperator::UploadFrameBufferedBufferData(const std::vector<BufferHandle>& buffers, const BufferUploadDesc& upload_desc)
    {
        if (buffers.empty())
        {
            return;
        }
        UploadBufferData(GetFrameBufferedBufferHandle(buffers), upload_desc);
    }

    std::vector<RenderTargetHandle> ResourceOperator::CreateFrameBufferedRenderTargets(const RenderTargetDesc& desc, const std::string& debug_name_prefix)
    {
        const auto frame_context = GetFrameContext();
        const unsigned frame_slot_count = frame_context.frame_slot_count;
        std::vector<RenderTargetHandle> render_targets;
        render_targets.reserve(frame_slot_count);

        std::string base_name = debug_name_prefix;
        if (base_name.empty())
        {
            base_name = desc.name.empty() ? "FrameBufferedRenderTarget" : desc.name;
        }

        for (unsigned frame_index = 0; frame_index < frame_slot_count; ++frame_index)
        {
            RenderTargetDesc frame_desc = desc;
            frame_desc.name = base_name + "_frame_" + std::to_string(frame_index);
            render_targets.push_back(CreateRenderTarget(frame_desc));
        }

        return render_targets;
    }

    RenderTargetHandle ResourceOperator::GetFrameBufferedRenderTargetHandle(const std::vector<RenderTargetHandle>& render_targets) const
    {
        return GetFrameBufferedRenderTargetHandle(render_targets, GetFrameContext());
    }

    RenderTargetHandle ResourceOperator::GetFrameBufferedRenderTargetHandle(const std::vector<RenderTargetHandle>& render_targets, const FrameContextSnapshot& frame_context) const
    {
        GLTF_CHECK(!render_targets.empty());
        const unsigned frame_slot = frame_context.per_frame_resource_binding_enabled
            ? (frame_context.frame_slot_index % static_cast<unsigned>(render_targets.size()))
            : 0u;
        return render_targets[frame_slot];
    }

    RenderTargetHandle ResourceOperator::CreateFrameBufferedRenderTargetAlias(const RenderTargetDesc& desc, const std::string& debug_name_prefix)
    {
        auto render_targets = CreateFrameBufferedRenderTargets(desc, debug_name_prefix);
        GLTF_CHECK(!render_targets.empty());
        const auto logical_handle = render_targets.front();
        m_frame_buffered_render_target_aliases[logical_handle] = std::move(render_targets);
        return logical_handle;
    }

    RenderTargetHandle ResourceOperator::CreateRenderTarget(const RenderTargetDesc& desc)
    {
        return m_resource_manager->CreateRenderTarget(desc);
    }

    bool ResourceOperator::RetireRenderTarget(RenderTargetHandle handle)
    {
        if (!m_resource_manager || !handle.IsValid())
        {
            return false;
        }

        if (TryRetireTrackedRenderTargetAlias(handle))
        {
            return true;
        }

        return m_resource_manager->RetireRenderTarget(handle);
    }

    bool ResourceOperator::RetireFrameBufferedRenderTargets(const std::vector<RenderTargetHandle>& render_targets)
    {
        if (!m_resource_manager || render_targets.empty())
        {
            return false;
        }

        if (TryRetireTrackedRenderTargetAlias(render_targets.front()))
        {
            return true;
        }

        bool retired_all = true;
        std::set<RenderTargetHandle> unique_handles;
        for (const auto handle : render_targets)
        {
            if (!handle.IsValid() || !unique_handles.insert(handle).second)
            {
                continue;
            }

            retired_all = m_resource_manager->RetireRenderTarget(handle) && retired_all;
        }

        return retired_all;
    }

    bool ResourceOperator::RetireFrameBufferedRenderTargetAlias(RenderTargetHandle logical_handle)
    {
        if (!m_resource_manager)
        {
            return false;
        }

        return TryRetireTrackedRenderTargetAlias(logical_handle);
    }

    RenderTargetHandle ResourceOperator::CreateRenderTarget(const std::string& name, unsigned width, unsigned height,
        PixelFormat format, RenderTargetClearValue clear_value, ResourceUsage usage)
    {
        RendererInterface::RenderTargetDesc render_target_desc{};
        render_target_desc.name = name;
        render_target_desc.size_mode = RenderTargetSizeMode::FIXED;
        render_target_desc.width = width;
        render_target_desc.height = height;
        render_target_desc.format = format;
        render_target_desc.clear = clear_value;
        render_target_desc.usage = usage; 
        return  m_resource_manager->CreateRenderTarget(render_target_desc);
    }

    RenderTargetHandle ResourceOperator::CreateFrameBufferedRenderTargetAlias(
        const std::string& name,
        unsigned width,
        unsigned height,
        PixelFormat format,
        RenderTargetClearValue clear_value,
        ResourceUsage usage)
    {
        RendererInterface::RenderTargetDesc render_target_desc{};
        render_target_desc.name = name;
        render_target_desc.size_mode = RenderTargetSizeMode::FIXED;
        render_target_desc.width = width;
        render_target_desc.height = height;
        render_target_desc.format = format;
        render_target_desc.clear = clear_value;
        render_target_desc.usage = usage;
        return CreateFrameBufferedRenderTargetAlias(render_target_desc, name);
    }

    RenderTargetHandle ResourceOperator::CreateWindowRelativeRenderTarget(const std::string& name,
        PixelFormat format, RenderTargetClearValue clear_value, ResourceUsage usage, float width_scale, float height_scale, unsigned min_width, unsigned min_height)
    {
        RendererInterface::RenderTargetDesc render_target_desc{};
        render_target_desc.name = name;
        render_target_desc.format = format;
        render_target_desc.clear = clear_value;
        render_target_desc.usage = usage;
        render_target_desc.size_mode = RenderTargetSizeMode::WINDOW_RELATIVE;
        render_target_desc.width_scale = width_scale;
        render_target_desc.height_scale = height_scale;
        render_target_desc.min_width = (std::max)(1u, min_width);
        render_target_desc.min_height = (std::max)(1u, min_height);

        const unsigned current_width = (std::max)(1u, GetCurrentRenderWidth());
        const unsigned current_height = (std::max)(1u, GetCurrentRenderHeight());
        render_target_desc.width = ComputeScaledExtent(current_width, render_target_desc.width_scale, render_target_desc.min_width);
        render_target_desc.height = ComputeScaledExtent(current_height, render_target_desc.height_scale, render_target_desc.min_height);
        return m_resource_manager->CreateRenderTarget(render_target_desc);
    }

    RenderTargetHandle ResourceOperator::CreateFrameBufferedWindowRelativeRenderTarget(
        const std::string& name,
        PixelFormat format,
        RenderTargetClearValue clear_value,
        ResourceUsage usage,
        float width_scale,
        float height_scale,
        unsigned min_width,
        unsigned min_height)
    {
        RendererInterface::RenderTargetDesc render_target_desc{};
        render_target_desc.name = name;
        render_target_desc.format = format;
        render_target_desc.clear = clear_value;
        render_target_desc.usage = usage;
        render_target_desc.size_mode = RenderTargetSizeMode::WINDOW_RELATIVE;
        render_target_desc.width_scale = width_scale;
        render_target_desc.height_scale = height_scale;
        render_target_desc.min_width = (std::max)(1u, min_width);
        render_target_desc.min_height = (std::max)(1u, min_height);

        const unsigned current_width = (std::max)(1u, GetCurrentRenderWidth());
        const unsigned current_height = (std::max)(1u, GetCurrentRenderHeight());
        render_target_desc.width = ComputeScaledExtent(current_width, render_target_desc.width_scale, render_target_desc.min_width);
        render_target_desc.height = ComputeScaledExtent(current_height, render_target_desc.height_scale, render_target_desc.min_height);
        return CreateFrameBufferedRenderTargetAlias(render_target_desc, name);
    }

    RenderPassHandle ResourceOperator::CreateRenderPass(const RenderPassDesc& desc)
    {
        std::shared_ptr<RenderPass> render_pass = std::make_shared<RenderPass>(desc);
        if (!render_pass->InitRenderPass(*m_resource_manager))
        {
            LOG_FORMAT_FLUSH("[RenderPass][Init][Error] Failed to initialize render pass (type=%d, shader_count=%zu, rt_binding_count=%zu).\n",
                             static_cast<int>(desc.type),
                             desc.shaders.size(),
                             desc.render_target_bindings.size());
            return NULL_HANDLE;
        }
        
        return InternalResourceHandleTable::Instance().RegisterRenderPass(render_pass);
    }

    RenderSceneHandle ResourceOperator::CreateRenderScene(const RenderSceneDesc& desc)
    {
        std::shared_ptr<RendererSceneGraph> scene_graph = std::make_shared<RendererSceneGraph>();
        
        glTFLoader loader;
        bool loaded = loader.LoadFile(desc.scene_file_name);
        GLTF_CHECK(loaded);
        
        bool added = scene_graph->InitializeRootNodeWithSceneFile_glTF(loader);
        GLTF_CHECK(added);

        const auto traverse_function = [](const RendererSceneNode& node)
        {
            return true;
        };
    
        scene_graph->GetRootNode().Traverse(traverse_function);
        
        return InternalResourceHandleTable::Instance().RegisterRenderScene(scene_graph);
    }

    IRHIDevice& ResourceOperator::GetDevice() const
    {
        return m_resource_manager->GetDevice();
    }

    unsigned ResourceOperator::GetCurrentBackBufferIndex() const
    {
        return m_resource_manager->GetCurrentBackBufferIndex();
    }

    unsigned ResourceOperator::GetCurrentFrameSlotIndex() const
    {
        return m_resource_manager->GetCurrentFrameSlotIndex();
    }

    unsigned ResourceOperator::GetFrameSlotCount() const
    {
        return m_resource_manager->GetFrameSlotCount();
    }

    unsigned ResourceOperator::GetSwapchainImageCount() const
    {
        return m_resource_manager->GetSwapchainImageCount();
    }

    unsigned ResourceOperator::GetBackBufferCount() const
    {
        return m_resource_manager->GetBackBufferCount();
    }

    FrameContextSnapshot ResourceOperator::GetFrameContext() const
    {
        FrameContextSnapshot frame_context = m_resource_manager->GetFrameContext();
        frame_context.per_frame_resource_binding_enabled = m_per_frame_resource_binding_enabled;
        frame_context.profiler_slot_count = frame_context.frame_slot_count;
        frame_context.profiler_slot_index = frame_context.frame_slot_index % frame_context.profiler_slot_count;
        return frame_context;
    }

    bool ResourceOperator::IsPerFrameResourceBindingEnabled() const
    {
        return m_per_frame_resource_binding_enabled;
    }

    void ResourceOperator::SetPerFrameResourceBindingEnabled(bool enable)
    {
        m_per_frame_resource_binding_enabled = enable;
    }

    void ResourceOperator::ApplyFrameBufferedRenderTargetAliases()
    {
        for (const auto& alias_pair : m_frame_buffered_render_target_aliases)
        {
            const auto logical_handle = alias_pair.first;
            const auto& candidates = alias_pair.second;
            if (candidates.empty())
            {
                continue;
            }

            const auto target_handle = GetFrameBufferedRenderTargetHandle(candidates);
            auto target_render_target = InternalResourceHandleTable::Instance().GetRenderTarget(target_handle);
            const bool updated = InternalResourceHandleTable::Instance().UpdateRenderTarget(logical_handle, target_render_target);
            GLTF_CHECK(updated);
            m_frame_buffered_render_target_alias_current[logical_handle] = target_handle;
        }
    }

    bool ResourceOperator::TryRetireTrackedRenderTargetAlias(RenderTargetHandle handle)
    {
        if (!m_resource_manager)
        {
            return false;
        }

        auto alias_it = m_frame_buffered_render_target_aliases.end();
        for (auto it = m_frame_buffered_render_target_aliases.begin(); it != m_frame_buffered_render_target_aliases.end(); ++it)
        {
            if (it->first == handle ||
                std::find(it->second.begin(), it->second.end(), handle) != it->second.end())
            {
                alias_it = it;
                break;
            }
        }

        if (alias_it == m_frame_buffered_render_target_aliases.end())
        {
            return false;
        }

        std::set<RenderTargetHandle> unique_handles(alias_it->second.begin(), alias_it->second.end());
        m_frame_buffered_render_target_alias_current.erase(alias_it->first);
        m_frame_buffered_render_target_aliases.erase(alias_it);

        bool retired_all = !unique_handles.empty();
        for (const auto target_handle : unique_handles)
        {
            if (!target_handle.IsValid())
            {
                continue;
            }

            retired_all = m_resource_manager->RetireRenderTarget(target_handle) && retired_all;
        }

        return retired_all;
    }

    IRHITextureDescriptorAllocation& ResourceOperator::GetCurrentSwapchainRT() const
    {
        return m_resource_manager->GetCurrentSwapchainRT();
    }

    IRHITextureDescriptorAllocation& ResourceOperator::GetCurrentSwapchainRT(const FrameContextSnapshot& frame_context) const
    {
        return m_resource_manager->GetCurrentSwapchainRT(frame_context);
    }

    bool ResourceOperator::HasCurrentSwapchainRT() const
    {
        return m_resource_manager->HasCurrentSwapchainRT();
    }

    void ResourceOperator::UploadBufferData(BufferHandle handle, const BufferUploadDesc& upload_desc)
    {
        auto buffer = InternalResourceHandleTable::Instance().GetBuffer(handle);
        m_resource_manager->GetMemoryManager().UploadBufferData(m_resource_manager->GetDevice(), m_resource_manager->GetCommandListForRecordPassCommand(), *buffer, upload_desc.data, 0, upload_desc.size);
    }

    void ResourceOperator::BeginFrame()
    {
        return m_resource_manager->BeginFrame();
    }

    void ResourceOperator::WaitFrameRenderFinished()
    {
        return m_resource_manager->WaitFrameRenderFinished();
    }

    void ResourceOperator::WaitGPUIdle()
    {
        return m_resource_manager->WaitGPUIdle();
    }

    void ResourceOperator::AdvanceFrameSlot()
    {
        return m_resource_manager->AdvanceFrameSlot();
    }

    void ResourceOperator::InvalidateSwapchainResizeRequest()
    {
        return m_resource_manager->InvalidateSwapchainResizeRequest();
    }

    WindowSurfaceSyncResult ResourceOperator::SyncWindowSurface(unsigned window_width, unsigned window_height)
    {
        return m_resource_manager->SyncWindowSurface(window_width, window_height);
    }

    void ResourceOperator::NotifySwapchainAcquireFailure()
    {
        return m_resource_manager->NotifySwapchainAcquireFailure();
    }

    void ResourceOperator::NotifySwapchainPresentFailure()
    {
        return m_resource_manager->NotifySwapchainPresentFailure();
    }

    bool ResourceOperator::ResizeSwapchainIfNeeded(unsigned width, unsigned height)
    {
        return m_resource_manager->ResizeSwapchainIfNeeded(width, height);
    }

    bool ResourceOperator::ResizeWindowDependentRenderTargets(unsigned width, unsigned height)
    {
        return m_resource_manager->ResizeWindowDependentRenderTargets(width, height);
    }

    SwapchainLifecycleState ResourceOperator::GetSwapchainLifecycleState() const
    {
        return m_resource_manager->GetSwapchainLifecycleState();
    }

    SwapchainResizePolicy ResourceOperator::GetSwapchainResizePolicy() const
    {
        return m_resource_manager->GetSwapchainResizePolicy();
    }

    void ResourceOperator::SetSwapchainResizePolicy(const SwapchainResizePolicy& policy, bool reset_retry_state)
    {
        return m_resource_manager->SetSwapchainResizePolicy(policy, reset_retry_state);
    }

    SwapchainPresentMode ResourceOperator::GetSwapchainPresentMode() const
    {
        return m_resource_manager->GetSwapchainPresentMode();
    }

    void ResourceOperator::SetSwapchainPresentMode(SwapchainPresentMode mode)
    {
        return m_resource_manager->SetSwapchainPresentMode(mode);
    }

    IRHICommandList& ResourceOperator::GetCommandListForRecordPassCommand(RenderPassHandle pass) const
    {
        return m_resource_manager->GetCommandListForRecordPassCommand(pass);
    }

    IRHICommandList& ResourceOperator::GetCommandListForRecordPassCommand(const FrameContextSnapshot& frame_context, RenderPassHandle pass) const
    {
        return m_resource_manager->GetCommandListForRecordPassCommand(frame_context, pass);
    }

    IRHIDescriptorManager& ResourceOperator::GetDescriptorManager() const
    {
        return m_resource_manager->GetMemoryManager().GetDescriptorManager();
    }

    IRHIMemoryManager& ResourceOperator::GetMemoryManager() const
    {
        return m_resource_manager->GetMemoryManager();
    }

    IRHICommandQueue& ResourceOperator::GetCommandQueue() const
    {
        return m_resource_manager->GetCommandQueue();
    }

    IRHISwapChain& ResourceOperator::GetCurrentSwapchain() const
    {
        return m_resource_manager->GetSwapChain();
    }

    unsigned ResourceOperator::GetCurrentRenderWidth() const
    {
        return m_resource_manager->GetSwapChain().GetWidth();
    }

    unsigned ResourceOperator::GetCurrentRenderHeight() const
    {
        return m_resource_manager->GetSwapChain().GetHeight();
    }

    bool ResourceOperator::CleanupAllResources(bool clear_window_handles)
    {
        if (!m_resource_manager)
        {
            return true;
        }

        m_resource_manager->WaitFrameRenderFinished();
        m_resource_manager->WaitGPUIdle();

        // Ensure profiler query pools are released while device/queue are still valid.
        // RenderGraph may outlive ResourceOperator during runtime-RHI switch.
        RHIUtilInstanceManager::Instance().ShutdownTimestampProfiler();

        auto& memory_manager = m_resource_manager->GetMemoryManager();
        // Release memory-manager-owned allocations first (especially Vulkan VMA allocations),
        // then release globally tracked RHI objects.
        const bool released_allocations = memory_manager.ReleaseAllResource();
        GLTF_CHECK(released_allocations);
        const bool cleaned_resources = RHIResourceFactory::CleanupResources(memory_manager);
        GLTF_CHECK(cleaned_resources);

        if (clear_window_handles)
        {
            InternalResourceHandleTable::Instance().ClearAll();
        }
        else
        {
            InternalResourceHandleTable::Instance().ClearRuntimeResources();
        }

        m_resource_manager.reset();
        m_render_passes.clear();
        m_frame_buffered_render_target_aliases.clear();
        m_frame_buffered_render_target_alias_current.clear();

        return cleaned_resources && released_allocations;
    }

    void RenderGraph::DeferredReleaseQueue::Enqueue(
        const std::shared_ptr<IRHIResource>& resource,
        unsigned long long current_frame,
        unsigned delay_frame)
    {
        if (!resource)
        {
            return;
        }

        const unsigned long long retire_frame = current_frame + delay_frame;
        if (!entries.empty() && entries.back().retire_frame == retire_frame)
        {
            entries.back().resources.push_back(resource);
            return;
        }

        DeferredReleaseEntry entry{};
        entry.retire_frame = retire_frame;
        entry.resources.push_back(resource);
        entries.push_back(std::move(entry));
    }

    void RenderGraph::DeferredReleaseQueue::EnqueueRetainedObject(
        std::shared_ptr<void> object,
        unsigned long long current_frame,
        unsigned delay_frame)
    {
        if (!object)
        {
            return;
        }

        const unsigned long long retire_frame = current_frame + delay_frame;
        if (!entries.empty() && entries.back().retire_frame == retire_frame)
        {
            entries.back().retained_objects.push_back(std::move(object));
            return;
        }

        DeferredReleaseEntry entry{};
        entry.retire_frame = retire_frame;
        entry.retained_objects.push_back(std::move(object));
        entries.push_back(std::move(entry));
    }

    void RenderGraph::DeferredReleaseQueue::Flush(
        IRHIMemoryManager& memory_manager,
        unsigned long long current_frame,
        bool force_release_all)
    {
        while (!entries.empty())
        {
            if (!force_release_all && entries.front().retire_frame > current_frame)
            {
                break;
            }

            auto entry = std::move(entries.front());
            entries.pop_front();
            for (const auto& resource : entry.resources)
            {
                const bool released = RHIResourceFactory::ReleaseResource(memory_manager, resource);
                GLTF_CHECK(released);
            }
        }
    }

    void RenderGraph::DeferredReleaseQueue::Clear()
    {
        entries.clear();
    }

    RenderGraph::RenderPassDescriptorResource* RenderGraph::DescriptorResourceStore::Find(RenderGraphNodeHandle node_handle)
    {
        const auto it = resources.find(node_handle);
        return it != resources.end() ? &it->second : nullptr;
    }

    const RenderGraph::RenderPassDescriptorResource* RenderGraph::DescriptorResourceStore::Find(RenderGraphNodeHandle node_handle) const
    {
        const auto it = resources.find(node_handle);
        return it != resources.end() ? &it->second : nullptr;
    }

    RenderGraph::RenderPassDescriptorResource& RenderGraph::DescriptorResourceStore::GetOrCreate(RenderGraphNodeHandle node_handle)
    {
        return resources[node_handle];
    }

    void RenderGraph::DescriptorResourceStore::MarkUsed(RenderGraphNodeHandle node_handle, unsigned long long frame_index)
    {
        last_used_frames[node_handle] = frame_index;
    }

    void RenderGraph::DescriptorResourceStore::Erase(RenderGraphNodeHandle node_handle)
    {
        resources.erase(node_handle);
        last_used_frames.erase(node_handle);
    }

    std::vector<RenderGraphNodeHandle> RenderGraph::DescriptorResourceStore::CollectExpiredInactiveHandles(
        const std::set<RenderGraphNodeHandle>& active_node_handles,
        unsigned long long current_frame,
        unsigned retention_frame) const
    {
        std::vector<RenderGraphNodeHandle> expired_handles;
        expired_handles.reserve(resources.size());

        for (const auto& resource_pair : resources)
        {
            const auto node_handle = resource_pair.first;
            if (active_node_handles.contains(node_handle))
            {
                continue;
            }

            const auto last_used_it = last_used_frames.find(node_handle);
            const unsigned long long last_used_frame =
                last_used_it != last_used_frames.end() ? last_used_it->second : 0u;
            if (current_frame < last_used_frame + retention_frame)
            {
                continue;
            }

            expired_handles.push_back(node_handle);
        }

        return expired_handles;
    }

    void RenderGraph::ExecutionPlanState::MarkDirty()
    {
        plan_dirty = true;
    }

    void RenderGraph::ExecutionPlanState::ResetCache()
    {
        cached_execution_signature = 0;
        cached_execution_node_count = 0;
        cached_execution_graph_valid = true;
        cached_execution_order.clear();
        plan_dirty = true;
    }

    bool RenderGraph::ExecutionPlanState::UpdateActiveNodeSet(
        std::size_t active_node_set_signature,
        std::size_t active_node_set_count)
    {
        const bool changed =
            active_node_set_signature != last_active_node_set_signature ||
            active_node_set_count != last_active_node_set_count;
        if (changed)
        {
            last_active_node_set_signature = active_node_set_signature;
            last_active_node_set_count = active_node_set_count;
        }

        return changed;
    }

    bool RenderGraph::ExecutionPlanState::CollectActiveNodes(
        const std::set<RenderGraphNodeHandle>& active_node_handles,
        std::vector<RenderGraphNodeHandle>& out_nodes)
    {
        out_nodes.clear();
        out_nodes.reserve(active_node_handles.size());

        std::size_t active_node_set_signature = 1469598103934665603ULL;
        for (const auto handle : active_node_handles)
        {
            out_nodes.push_back(handle);
            HashCombine(active_node_set_signature, handle.value);
        }

        return UpdateActiveNodeSet(active_node_set_signature, out_nodes.size());
    }

    RenderGraph::ExecutionPlanContext RenderGraph::ExecutionPlanState::BuildContext(
        const std::vector<RenderGraphNodeHandle>& nodes,
        const std::vector<RenderGraphNodeDesc>& render_graph_nodes,
        const std::set<RenderGraphNodeHandle>& registered_nodes) const
    {
        return ExecutionPlanContext{
            nodes,
            render_graph_nodes,
            registered_nodes,
            cached_execution_order,
            cached_execution_graph_valid,
            cached_execution_signature,
            cached_execution_node_count
        };
    }

    RenderGraph::ExecutionPlanCacheState RenderGraph::ExecutionPlanState::BuildCacheState()
    {
        return ExecutionPlanCacheState{
            cached_execution_graph_valid,
            cached_execution_order,
            cached_execution_signature,
            cached_execution_node_count
        };
    }

    bool RenderGraph::ExecutionPlanState::IsCachedExecutionOrderMissing() const
    {
        return cached_execution_graph_valid && cached_execution_order.empty();
    }

    bool RenderGraph::ExecutionPlanState::ShouldRebuild(
        bool active_node_set_changed,
        bool should_update_dependency_diagnostics) const
    {
        return plan_dirty ||
            active_node_set_changed ||
            IsCachedExecutionOrderMissing() ||
            should_update_dependency_diagnostics;
    }

    void RenderGraph::ExecutionPlanState::MarkPlanApplied()
    {
        plan_dirty = false;
    }

    void RenderGraph::DependencyDiagnosticsState::Reset()
    {
        last_update_frame_index = 0;
        frame_slot_resource_access_snapshots.clear();
        frame_slot_resource_access_snapshot_valid.clear();
    }

    bool RenderGraph::DependencyDiagnosticsState::ShouldUpdate(
        unsigned long long current_frame,
        unsigned interval_frames) const
    {
        return (last_update_frame_index == 0u) ||
            (current_frame <= last_update_frame_index) ||
            ((current_frame - last_update_frame_index) >= static_cast<unsigned long long>(interval_frames));
    }

    void RenderGraph::DependencyDiagnosticsState::MarkUpdated(unsigned long long current_frame)
    {
        last_update_frame_index = current_frame;
    }

    void RenderGraph::DependencyDiagnosticsState::EnsureCrossFrameHazardSnapshotStorage(unsigned window_size)
    {
        if (frame_slot_resource_access_snapshots.size() == static_cast<std::size_t>(window_size) &&
            frame_slot_resource_access_snapshot_valid.size() == static_cast<std::size_t>(window_size))
        {
            return;
        }

        frame_slot_resource_access_snapshots.clear();
        frame_slot_resource_access_snapshots.resize(window_size);
        frame_slot_resource_access_snapshot_valid.assign(window_size, 0u);
    }

    const RenderGraph::FrameResourceAccessSnapshot* RenderGraph::DependencyDiagnosticsState::GetCrossFrameHazardSnapshot(
        unsigned hazard_slot_index) const
    {
        if (hazard_slot_index >= frame_slot_resource_access_snapshot_valid.size() ||
            hazard_slot_index >= frame_slot_resource_access_snapshots.size() ||
            frame_slot_resource_access_snapshot_valid[hazard_slot_index] == 0u)
        {
            return nullptr;
        }

        return &frame_slot_resource_access_snapshots[hazard_slot_index];
    }

    void RenderGraph::DependencyDiagnosticsState::UpdateCrossFrameHazardSnapshot(
        unsigned hazard_slot_index,
        FrameResourceAccessSnapshot snapshot)
    {
        GLTF_CHECK(hazard_slot_index < frame_slot_resource_access_snapshots.size());
        GLTF_CHECK(hazard_slot_index < frame_slot_resource_access_snapshot_valid.size());
        frame_slot_resource_access_snapshots[hazard_slot_index] = std::move(snapshot);
        frame_slot_resource_access_snapshot_valid[hazard_slot_index] = 1u;
    }

    RenderGraph::RenderGraph(ResourceOperator& allocator, RenderWindow& window, bool enable_debug_ui)
        : m_resource_allocator(allocator)
        , m_window(window)
    {
        m_debug_ui_enabled = enable_debug_ui;
        m_validation_policy.log_interval_frames = (std::max)(1u, m_validation_policy.log_interval_frames);
        m_validation_policy.cross_frame_hazard_check_interval_frames =
            (std::max)(1u, m_validation_policy.cross_frame_hazard_check_interval_frames);
        if (m_debug_ui_enabled)
        {
            GLTF_CHECK(InitDebugUI());
        }
        GLTF_CHECK(InitGPUProfiler());
    }

    RenderGraph::~RenderGraph()
    {
        ShutdownRuntimeServices();
    }

    RenderGraphNodeHandle RenderGraph::CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc)
    {
        RenderGraphNodeHandle result{static_cast<unsigned>(m_render_graph_nodes.size())};
        m_render_graph_nodes.push_back(render_graph_node_desc);
        m_execution_plan_state.MarkDirty();
        return result;
    }

    bool RenderGraph::BuildRenderGraphNodeFromSetup(
        ResourceOperator& allocator,
        const RenderPassSetupInfo& setup_info,
        RenderGraphNodeDesc& out_render_graph_node_desc,
        std::tuple<unsigned, unsigned, unsigned>& out_pruned_binding_counts)
    {
        RendererInterface::RenderPassDesc render_pass_desc{};
        render_pass_desc.type = setup_info.render_pass_type;
        render_pass_desc.render_state = setup_info.render_state;

        if (setup_info.render_pass_type == RendererInterface::RenderPassType::RAY_TRACING &&
            !RHIUtilInstanceManager::Instance().SupportRayTracing(allocator.GetDevice()))
        {
            const char* group_name = setup_info.debug_group.empty() ? "<group-empty>" : setup_info.debug_group.c_str();
            const char* pass_name = setup_info.debug_name.empty() ? "<pass-empty>" : setup_info.debug_name.c_str();
            LOG_FORMAT_FLUSH(
                "[RenderGraph][Validation] Pass (%s/%s) requires ray-tracing pipeline support, but the current device was not created with that capability.\n",
                group_name,
                pass_name);
            return false;
        }
    
        for (const auto& shader_info : setup_info.shader_setup_infos)
        {
            RendererInterface::ShaderDesc shader_desc{};
            shader_desc.shader_type = shader_info.shader_type; 
            shader_desc.entry_point = shader_info.entry_function;
            shader_desc.shader_file_name = shader_info.shader_file;
            auto shader_handle = allocator.CreateShader(shader_desc);
            
            render_pass_desc.shaders.emplace(shader_info.shader_type, shader_handle);
        }
    
        RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
        for (const auto& module : setup_info.modules)
        {
            module->BindDrawCommands(render_pass_draw_desc);
        }

        switch (setup_info.render_pass_type)
        {
        case RendererInterface::RenderPassType::GRAPHICS:
            for (const auto& render_target : setup_info.render_targets)
            {
                render_pass_desc.render_target_bindings.push_back(render_target.second);
                render_pass_draw_desc.render_target_resources.emplace(render_target.first, render_target.second);
            }
            break;
        case RendererInterface::RenderPassType::COMPUTE:
            break;
        case RendererInterface::RenderPassType::RAY_TRACING:
            break;
        }
    
        for (const auto& render_target : setup_info.sampled_render_targets)
        {
            render_pass_draw_desc.render_target_texture_resources[render_target.name] = render_target;
        }

        render_pass_draw_desc.texture_resources.insert(setup_info.texture_resources.begin(), setup_info.texture_resources.end());
        render_pass_draw_desc.buffer_resources.insert(setup_info.buffer_resources.begin(), setup_info.buffer_resources.end());

        auto exclude_named_bindings = [](auto& resource_map, const std::set<std::string>& excluded_binding_names)
        {
            unsigned removed_count = 0;
            for (const auto& binding_name : excluded_binding_names)
            {
                removed_count += static_cast<unsigned>(resource_map.erase(binding_name));
            }
            return removed_count;
        };

        const unsigned excluded_buffer_count =
            exclude_named_bindings(render_pass_draw_desc.buffer_resources, setup_info.excluded_buffer_bindings);
        const unsigned excluded_texture_count =
            exclude_named_bindings(render_pass_draw_desc.texture_resources, setup_info.excluded_texture_bindings);
        const unsigned excluded_render_target_texture_count =
            exclude_named_bindings(render_pass_draw_desc.render_target_texture_resources, setup_info.excluded_render_target_texture_bindings);
        const unsigned total_excluded_binding_count =
            excluded_buffer_count + excluded_texture_count + excluded_render_target_texture_count;
        if (total_excluded_binding_count > 0)
        {
            const char* group_name = setup_info.debug_group.empty() ? "<group-empty>" : setup_info.debug_group.c_str();
            const char* pass_name = setup_info.debug_name.empty() ? "<pass-empty>" : setup_info.debug_name.c_str();
            LOG_FORMAT_FLUSH("[RenderGraph][Setup] Pass (%s/%s) excluded bindings by policy: buffer=%u, texture=%u, render_target_texture=%u.\n",
                             group_name,
                             pass_name,
                             excluded_buffer_count,
                             excluded_texture_count,
                             excluded_render_target_texture_count);
        }
    
        if (setup_info.execute_command.has_value())
        {
            render_pass_draw_desc.execute_commands.push_back(setup_info.execute_command.value());    
        }
        render_pass_draw_desc.execute_commands.insert(
            render_pass_draw_desc.execute_commands.end(),
            setup_info.execute_commands.begin(),
            setup_info.execute_commands.end());

        render_pass_desc.viewport_width = setup_info.viewport_width;
        render_pass_desc.viewport_height = setup_info.viewport_height;
        
        auto render_pass_handle = allocator.CreateRenderPass(render_pass_desc);
        if (!render_pass_handle.IsValid())
        {
            return false;
        }

        unsigned pruned_buffer_count = 0;
        unsigned pruned_texture_count = 0;
        unsigned pruned_render_target_texture_count = 0;
        auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(render_pass_handle);
        if (render_pass)
        {
            auto prune_unmapped_named_resources = [&render_pass](auto& resource_map)
            {
                unsigned pruned_count = 0;
                for (auto it = resource_map.begin(); it != resource_map.end(); )
                {
                    if (!render_pass->HasRootSignatureAllocation(it->first))
                    {
                        it = resource_map.erase(it);
                        ++pruned_count;
                    }
                    else
                    {
                        ++it;
                    }
                }
                return pruned_count;
            };

            pruned_buffer_count = prune_unmapped_named_resources(render_pass_draw_desc.buffer_resources);
            pruned_texture_count = prune_unmapped_named_resources(render_pass_draw_desc.texture_resources);
            pruned_render_target_texture_count = prune_unmapped_named_resources(render_pass_draw_desc.render_target_texture_resources);
        }

        out_render_graph_node_desc = RendererInterface::RenderGraphNodeDesc{};
        out_render_graph_node_desc.draw_info = std::move(render_pass_draw_desc);
        out_render_graph_node_desc.render_pass_handle = render_pass_handle;
        out_render_graph_node_desc.render_state = setup_info.render_state;
        out_render_graph_node_desc.dependency_render_graph_nodes = setup_info.dependency_render_graph_nodes;
        out_render_graph_node_desc.pre_render_callback = setup_info.pre_render_callback;
        out_render_graph_node_desc.debug_group = setup_info.debug_group;
        out_render_graph_node_desc.debug_name = setup_info.debug_name;
        out_pruned_binding_counts =
            std::make_tuple(pruned_buffer_count, pruned_texture_count, pruned_render_target_texture_count);
        return true;
    }

    void RenderGraph::SyncAutoPrunedNamedBindingCounts(
        RenderGraphNodeHandle render_graph_node_handle,
        const RenderGraphNodeDesc& render_graph_node_desc,
        const std::tuple<unsigned, unsigned, unsigned>& pruned_binding_counts)
    {
        const auto [pruned_buffer_count, pruned_texture_count, pruned_render_target_texture_count] =
            pruned_binding_counts;
        const unsigned total_pruned_bindings =
            pruned_buffer_count + pruned_texture_count + pruned_render_target_texture_count;
        if (total_pruned_bindings == 0)
        {
            m_auto_pruned_named_binding_counts.erase(render_graph_node_handle);
            return;
        }

        m_auto_pruned_named_binding_counts[render_graph_node_handle] = pruned_binding_counts;
        const char* group_name = render_graph_node_desc.debug_group.empty() ? "<group-empty>" : render_graph_node_desc.debug_group.c_str();
        const char* pass_name = render_graph_node_desc.debug_name.empty() ? "<pass-empty>" : render_graph_node_desc.debug_name.c_str();
        LOG_FORMAT_FLUSH("[RenderGraph][Validation] Node %u (%s/%s) auto-pruned unmapped bindings: buffer=%u, texture=%u, render_target_texture=%u.\n",
                         render_graph_node_handle.value,
                         group_name,
                         pass_name,
                         pruned_buffer_count,
                         pruned_texture_count,
                         pruned_render_target_texture_count);
    }

    void RenderGraph::RetireRenderGraphNodeResources(
        RenderGraphNodeHandle render_graph_node_handle,
        const FrameContextSnapshot& frame_context)
    {
        auto* descriptor_resource = m_descriptor_resource_store.Find(render_graph_node_handle);
        if (descriptor_resource)
        {
            ReleaseRenderPassDescriptorResource(*descriptor_resource, frame_context);
            m_descriptor_resource_store.Erase(render_graph_node_handle);
        }

        const auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        auto retired_render_pass = InternalResourceHandleTable::Instance().RemoveRenderPass(node_desc.render_pass_handle);
        if (retired_render_pass)
        {
            EnqueueRetainedObjectForDeferredRelease(std::move(retired_render_pass), frame_context);
        }
    }

    RenderGraphNodeHandle RenderGraph::CreateRenderGraphNode(ResourceOperator& allocator,const RenderPassSetupInfo& setup_info)
    {
        RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
        std::tuple<unsigned, unsigned, unsigned> pruned_binding_counts{};
        if (!BuildRenderGraphNodeFromSetup(allocator, setup_info, render_graph_node_desc, pruned_binding_counts))
        {
            return NULL_HANDLE;
        }

        auto render_graph_node_handle = CreateRenderGraphNode(render_graph_node_desc);
        SyncAutoPrunedNamedBindingCounts(render_graph_node_handle, render_graph_node_desc, pruned_binding_counts);
        return render_graph_node_handle;
    }

    bool RenderGraph::RebuildRenderGraphNode(
        ResourceOperator& allocator,
        RenderGraphNodeHandle render_graph_node_handle,
        const RenderPassSetupInfo& setup_info)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());

        RendererInterface::RenderGraphNodeDesc rebuilt_node_desc{};
        std::tuple<unsigned, unsigned, unsigned> pruned_binding_counts{};
        if (!BuildRenderGraphNodeFromSetup(allocator, setup_info, rebuilt_node_desc, pruned_binding_counts))
        {
            return false;
        }

        const auto frame_context = m_resource_allocator.GetFrameContext();
        RetireRenderGraphNodeResources(render_graph_node_handle, frame_context);
        m_render_graph_nodes[render_graph_node_handle.value] = std::move(rebuilt_node_desc);
        m_pending_render_state_updates.erase(render_graph_node_handle);
        m_render_pass_validation_last_log_frame.erase(render_graph_node_handle);
        m_render_pass_validation_last_message_hash.erase(render_graph_node_handle);
        SyncAutoPrunedNamedBindingCounts(
            render_graph_node_handle,
            m_render_graph_nodes[render_graph_node_handle.value],
            pruned_binding_counts);
        m_dependency_diagnostics_state.Reset();
        m_execution_plan_state.MarkDirty();
        return true;
    }

    bool RenderGraph::RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        GLTF_CHECK(!m_render_graph_node_handles.contains(render_graph_node_handle));
        m_render_graph_node_handles.insert(render_graph_node_handle);
        m_dependency_diagnostics_state.Reset();
        m_execution_plan_state.MarkDirty();
        return true;
    }

    bool RenderGraph::RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        const auto frame_context = m_resource_allocator.GetFrameContext();
        m_render_graph_node_handles.erase(render_graph_node_handle);
        m_pending_render_state_updates.erase(render_graph_node_handle);
        RetireRenderGraphNodeResources(render_graph_node_handle, frame_context);
        m_render_graph_nodes[render_graph_node_handle.value] = RenderGraphNodeDesc{};

        m_auto_pruned_named_binding_counts.erase(render_graph_node_handle);
        m_render_pass_validation_last_log_frame.erase(render_graph_node_handle);
        m_render_pass_validation_last_message_hash.erase(render_graph_node_handle);

        m_dependency_diagnostics_state.Reset();
        m_execution_plan_state.ResetCache();
        return true;
    }

    bool RenderGraph::UpdateComputeDispatch(RenderGraphNodeHandle render_graph_node_handle, unsigned group_size_x, unsigned group_size_y, unsigned group_size_z)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());

        auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        for (auto& command : node_desc.draw_info.execute_commands)
        {
            if (command.type != ExecuteCommandType::COMPUTE_DISPATCH_COMMAND)
            {
                continue;
            }

            command.parameter.dispatch_parameter.group_size_x = group_size_x;
            command.parameter.dispatch_parameter.group_size_y = group_size_y;
            command.parameter.dispatch_parameter.group_size_z = group_size_z;
            m_execution_plan_state.MarkDirty();
            return true;
        }

        return false;
    }

    bool RenderGraph::QueueNodeRenderStateUpdate(RenderGraphNodeHandle render_graph_node_handle, const RenderStateDesc& render_state)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());

        const auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(node_desc.render_pass_handle);
        if (!render_pass || render_pass->GetRenderPassType() != RenderPassType::GRAPHICS)
        {
            return false;
        }

        const auto pending_it = m_pending_render_state_updates.find(render_graph_node_handle);
        if (pending_it != m_pending_render_state_updates.end())
        {
            if (IsEquivalentRenderStateDesc(pending_it->second.render_state, render_state))
            {
                return true;
            }
        }
        else if (IsEquivalentRenderStateDesc(node_desc.render_state, render_state))
        {
            return true;
        }

        m_pending_render_state_updates[render_graph_node_handle] = PendingRenderStateUpdate{.render_state = render_state};
        return true;
    }

    bool RenderGraph::UpdateNodeDependencies(
        RenderGraphNodeHandle render_graph_node_handle,
        const std::vector<RenderGraphNodeHandle>& dependency_render_graph_nodes)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());

        for (const auto dependency_handle : dependency_render_graph_nodes)
        {
            GLTF_CHECK(dependency_handle.IsValid());
            GLTF_CHECK(dependency_handle.value < m_render_graph_nodes.size());
            if (dependency_handle == render_graph_node_handle)
            {
                return false;
            }
        }

        auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        if (node_desc.dependency_render_graph_nodes == dependency_render_graph_nodes)
        {
            return true;
        }

        node_desc.dependency_render_graph_nodes = dependency_render_graph_nodes;
        m_dependency_diagnostics_state.Reset();
        m_execution_plan_state.MarkDirty();
        return true;
    }

    bool RenderGraph::UpdateNodeBufferBinding(RenderGraphNodeHandle render_graph_node_handle, const std::string& binding_name, BufferHandle buffer_handle)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        GLTF_CHECK(buffer_handle != NULL_HANDLE);

        auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        auto binding_it = node_desc.draw_info.buffer_resources.find(binding_name);
        if (binding_it == node_desc.draw_info.buffer_resources.end())
        {
            return false;
        }

        binding_it->second.buffer_handle = buffer_handle;
        m_dependency_diagnostics_state.Reset();
        m_execution_plan_state.MarkDirty();
        return true;
    }

    bool RenderGraph::UpdateNodeRenderTargetBinding(
        RenderGraphNodeHandle render_graph_node_handle,
        RenderTargetHandle old_render_target_handle,
        RenderTargetHandle new_render_target_handle)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        GLTF_CHECK(old_render_target_handle != NULL_HANDLE);
        GLTF_CHECK(new_render_target_handle != NULL_HANDLE);

        auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        auto binding_it = node_desc.draw_info.render_target_resources.find(old_render_target_handle);
        if (binding_it == node_desc.draw_info.render_target_resources.end())
        {
            return false;
        }

        if (old_render_target_handle == new_render_target_handle)
        {
            return true;
        }

        if (node_desc.draw_info.render_target_resources.contains(new_render_target_handle))
        {
            return false;
        }

        const RenderTargetBindingDesc binding_desc = binding_it->second;
        node_desc.draw_info.render_target_resources.erase(binding_it);
        node_desc.draw_info.render_target_resources.emplace(new_render_target_handle, binding_desc);
        m_dependency_diagnostics_state.Reset();
        m_execution_plan_state.MarkDirty();
        return true;
    }

    bool RenderGraph::UpdateNodeRenderTargetTextureBinding(
        RenderGraphNodeHandle render_graph_node_handle,
        const std::string& binding_name,
        const std::vector<RenderTargetHandle>& render_target_handles)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        if (render_target_handles.empty())
        {
            return false;
        }

        for (const auto render_target_handle : render_target_handles)
        {
            GLTF_CHECK(render_target_handle != NULL_HANDLE);
        }

        auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        auto binding_it = node_desc.draw_info.render_target_texture_resources.find(binding_name);
        if (binding_it == node_desc.draw_info.render_target_texture_resources.end())
        {
            return false;
        }

        binding_it->second.render_target_texture = render_target_handles;
        m_dependency_diagnostics_state.Reset();
        m_execution_plan_state.MarkDirty();
        return true;
    }

    bool RenderGraph::UpdateNodeRenderTargetTextureBinding(
        RenderGraphNodeHandle render_graph_node_handle,
        const std::string& binding_name,
        RenderTargetHandle render_target_handle)
    {
        return UpdateNodeRenderTargetTextureBinding(
            render_graph_node_handle,
            binding_name,
            std::vector<RenderTargetHandle>{render_target_handle});
    }

    bool RenderGraph::CompileRenderPassAndExecute()
    {
        m_window.RegisterTickCallback([this](unsigned long long interval)
        {
            OnFrameTick(interval);
        });

        return true;
    }

    void RenderGraph::OnFrameTick(unsigned long long interval)
    {
        m_current_frame_timing_breakdown = {};
        const auto frame_begin = std::chrono::steady_clock::now();

        FramePreparationContext frame_context{};
        const auto prepare_begin = std::chrono::steady_clock::now();
        if (!PrepareFrameForRendering(interval, frame_context))
        {
            const auto prepare_end = std::chrono::steady_clock::now();
            m_current_frame_timing_breakdown.prepare_frame_ms = ToMilliseconds(prepare_begin, prepare_end);
            const auto frame_end = std::chrono::steady_clock::now();
            m_current_frame_timing_breakdown.frame_index = m_frame_index;
            m_current_frame_timing_breakdown.frame_total_ms = ToMilliseconds(frame_begin, frame_end);
            m_current_frame_timing_breakdown.non_pass_cpu_ms = m_current_frame_timing_breakdown.frame_total_ms;
            m_current_frame_timing_breakdown.untracked_ms =
                (std::max)(0.0f, m_current_frame_timing_breakdown.frame_total_ms - m_current_frame_timing_breakdown.prepare_frame_ms);
            m_current_frame_timing_breakdown.valid = false;
            m_last_frame_timing_breakdown = m_current_frame_timing_breakdown;
            return;
        }
        const auto prepare_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.prepare_frame_ms = ToMilliseconds(prepare_begin, prepare_end);
        m_current_frame_timing_breakdown.frame_index = m_frame_index;

        const auto execute_begin = std::chrono::steady_clock::now();
        ExecuteRenderGraphFrame(frame_context, interval);
        const auto execute_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.execute_render_graph_ms = ToMilliseconds(execute_begin, execute_end);

        GLTF_CHECK(frame_context.command_list);
        const bool swapchain_ready = AcquireCurrentFrameSwapchain(frame_context);
        if (swapchain_ready)
        {
            const auto blit_begin = std::chrono::steady_clock::now();
            BlitFinalOutputToSwapchain(
                *frame_context.command_list,
                frame_context.presentation_frame_context,
                frame_context.window_width,
                frame_context.window_height);
            const auto blit_end = std::chrono::steady_clock::now();
            m_current_frame_timing_breakdown.blit_to_swapchain_ms = ToMilliseconds(blit_begin, blit_end);
        }

        const auto finalize_begin = std::chrono::steady_clock::now();
        FinalizeFrameSubmission(frame_context, swapchain_ready);
        const auto finalize_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.finalize_submission_ms = ToMilliseconds(finalize_begin, finalize_end);

        const auto frame_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.frame_total_ms = ToMilliseconds(frame_begin, frame_end);
        m_current_frame_timing_breakdown.non_pass_cpu_ms = (std::max)(
            0.0f,
            m_current_frame_timing_breakdown.frame_total_ms - m_current_frame_timing_breakdown.execute_passes_ms);
        const float tracked_frame_section_ms =
            m_current_frame_timing_breakdown.prepare_frame_ms +
            m_current_frame_timing_breakdown.execute_render_graph_ms +
            m_current_frame_timing_breakdown.blit_to_swapchain_ms +
            m_current_frame_timing_breakdown.finalize_submission_ms;
        m_current_frame_timing_breakdown.untracked_ms = (std::max)(
            0.0f,
            m_current_frame_timing_breakdown.frame_total_ms - tracked_frame_section_ms);
        m_current_frame_timing_breakdown.frame_wait_total_ms =
            m_current_frame_timing_breakdown.wait_previous_frame_ms +
            m_current_frame_timing_breakdown.acquire_command_list_ms +
            m_current_frame_timing_breakdown.acquire_swapchain_ms +
            m_current_frame_timing_breakdown.submit_command_list_ms +
            m_current_frame_timing_breakdown.present_call_ms;
        m_current_frame_timing_breakdown.valid = true;
        m_last_frame_timing_breakdown = m_current_frame_timing_breakdown;
    }

    bool RenderGraph::ResolveFinalColorOutput()
    {
        m_final_color_output = nullptr;
        if (m_final_color_output_render_target_handle != NULL_HANDLE)
        {
            auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(m_final_color_output_render_target_handle);
            m_final_color_output = render_target->m_source;
        }
        else if (m_final_color_output_texture_handle != NULL_HANDLE)
        {
            auto texture = InternalResourceHandleTable::Instance().GetTexture(m_final_color_output_texture_handle);
            m_final_color_output = texture->m_texture;
        }

        if (!m_final_color_output)
        {
            if (!m_missing_final_color_output_logged)
            {
                LOG_FORMAT_FLUSH(
                    "[RenderGraph] Missing final color output. Register a texture/render target via RegisterTextureToColorOutput/RegisterRenderTargetToColorOutput.\n");
                m_missing_final_color_output_logged = true;
            }
            return false;
        }

        m_missing_final_color_output_logged = false;
        return true;
    }

    void RenderGraph::ExecuteTickAndDebugUI(unsigned long long interval)
    {
        const auto tick_and_ui_begin = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.tick_callback_ms = 0.0f;
        m_current_frame_timing_breakdown.tick_other_ms = 0.0f;
        m_current_frame_timing_breakdown.module_tick_ms = 0.0f;
        m_current_frame_timing_breakdown.system_tick_ms = 0.0f;
        m_current_frame_timing_breakdown.debug_ui_build_ms = 0.0f;

        if (m_tick_callback)
        {
            const auto tick_begin = std::chrono::steady_clock::now();
            m_tick_callback(interval);
            const auto tick_end = std::chrono::steady_clock::now();
            m_current_frame_timing_breakdown.tick_callback_ms = ToMilliseconds(tick_begin, tick_end);
        }

        if (m_debug_ui_enabled && m_debug_ui_initialized)
        {
            const auto debug_ui_build_begin = std::chrono::steady_clock::now();
            GLTF_CHECK(RHIUtilInstanceManager::Instance().NewGUIFrame());
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (m_debug_ui_callback)
            {
                m_debug_ui_callback();
            }
            else
            {
                const bool framework_window_visible = ImGui::Begin("Renderer Framework");
                if (framework_window_visible)
                {
                    DrawFrameworkDebugUI();
                }
                ImGui::End();
            }
            const auto debug_ui_build_end = std::chrono::steady_clock::now();
            m_current_frame_timing_breakdown.debug_ui_build_ms =
                ToMilliseconds(debug_ui_build_begin, debug_ui_build_end);
        }

        const auto tick_and_ui_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.tick_and_debug_ui_build_ms =
            ToMilliseconds(tick_and_ui_begin, tick_and_ui_end);
    }

    bool RenderGraph::SyncWindowSurfaceAndAdvanceFrame(FramePreparationContext& frame_context, unsigned long long interval)
    {
        (void)interval;
        m_resource_allocator.BeginFrame();

        const auto surface_sync_begin = std::chrono::steady_clock::now();
        const auto surface_sync_result = m_resource_allocator.SyncWindowSurface(frame_context.window_width, frame_context.window_height);
        const auto surface_sync_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.sync_window_surface_ms = ToMilliseconds(surface_sync_begin, surface_sync_end);
        if (surface_sync_result.status == WindowSurfaceSyncStatus::MINIMIZED)
        {
            return false;
        }
        if (surface_sync_result.status == WindowSurfaceSyncStatus::INVALID || !m_resource_allocator.HasCurrentSwapchainRT())
        {
            m_resource_allocator.InvalidateSwapchainResizeRequest();
            return false;
        }

        m_resource_allocator.AdvanceFrameSlot();
        frame_context.resource_frame_context = m_resource_allocator.GetFrameContext();

        frame_context.require_explicit_frame_wait =
            surface_sync_result.status != WindowSurfaceSyncStatus::RESIZED &&
            RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12;
        const auto deferred_release_begin = std::chrono::steady_clock::now();
        ++m_frame_index;
        FlushDeferredResourceReleases(false);
        const auto deferred_release_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.deferred_release_ms = ToMilliseconds(deferred_release_begin, deferred_release_end);

        return true;
    }

    bool RenderGraph::AcquireCurrentFrameCommandContext(FramePreparationContext& frame_context)
    {
        const auto acquire_context_begin = std::chrono::steady_clock::now();
        frame_context.profiler_slot_index = frame_context.resource_frame_context.profiler_slot_index;

        const auto resolve_profiler_begin = std::chrono::steady_clock::now();
        ResolveGPUProfilerFrame(frame_context.profiler_slot_index);
        const auto resolve_profiler_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.resolve_gpu_profiler_ms = ToMilliseconds(resolve_profiler_begin, resolve_profiler_end);

        if (!m_resource_allocator.HasCurrentSwapchainRT())
        {
            frame_context.command_list = nullptr;
            m_current_frame_timing_breakdown.acquire_context_ms =
                ToMilliseconds(acquire_context_begin, std::chrono::steady_clock::now());
            return false;
        }

        if (frame_context.require_explicit_frame_wait)
        {
            const auto wait_begin = std::chrono::steady_clock::now();
            m_resource_allocator.WaitFrameRenderFinished();
            const auto wait_end = std::chrono::steady_clock::now();
            m_current_frame_timing_breakdown.wait_previous_frame_ms = ToMilliseconds(wait_begin, wait_end);
        }

        const auto acquire_command_list_begin = std::chrono::steady_clock::now();
        frame_context.command_list = &m_resource_allocator.GetCommandListForRecordPassCommand(frame_context.resource_frame_context);
        const auto acquire_command_list_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.acquire_command_list_ms =
            ToMilliseconds(acquire_command_list_begin, acquire_command_list_end);

        m_current_frame_timing_breakdown.acquire_context_ms =
            ToMilliseconds(acquire_context_begin, std::chrono::steady_clock::now());
        return true;
    }

    bool RenderGraph::PrepareFrameForRendering(unsigned long long interval, FramePreparationContext& frame_context)
    {
        if (!ResolveFinalColorOutput())
        {
            return false;
        }

        frame_context.window_width = m_window.GetWidth();
        frame_context.window_height = m_window.GetHeight();
        if (frame_context.window_width == 0 || frame_context.window_height == 0)
        {
            return false;
        }

        if (!SyncWindowSurfaceAndAdvanceFrame(frame_context, interval))
        {
            return false;
        }

        ApplyPendingRenderStateUpdates(frame_context.resource_frame_context);
        if (!AcquireCurrentFrameCommandContext(frame_context))
        {
            return false;
        }

        ExecuteTickAndDebugUI(interval);
        return true;
    }

    void RenderGraph::ExecuteRenderGraphFrame(FramePreparationContext& frame_context, unsigned long long interval)
    {
        BeginRenderDocFrameCapture();
        const auto planning_begin = std::chrono::steady_clock::now();
        m_resource_allocator.ApplyFrameBufferedRenderTargetAliases();
        if (m_final_color_output_render_target_handle != NULL_HANDLE)
        {
            auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(m_final_color_output_render_target_handle);
            m_final_color_output = render_target->m_source;
        }

        std::vector<RenderGraphNodeHandle> nodes;
        const bool active_node_set_changed =
            m_execution_plan_state.CollectActiveNodes(m_render_graph_node_handles, nodes);

        const ExecutionPlanContext plan_context =
            m_execution_plan_state.BuildContext(nodes, m_render_graph_nodes, m_render_graph_node_handles);

        const unsigned hazard_check_interval_frames =
            (std::max)(1u, m_validation_policy.cross_frame_hazard_check_interval_frames);
        const bool should_update_dependency_diagnostics =
            m_dependency_diagnostics_state.ShouldUpdate(m_frame_index, hazard_check_interval_frames);
        const bool should_rebuild_execution_plan =
            m_execution_plan_state.ShouldRebuild(active_node_set_changed, should_update_dependency_diagnostics);

        ExecutionPlanBuildResult plan{};
        ExecutionPlanCacheState execution_cache_state = m_execution_plan_state.BuildCacheState();
        std::vector<RenderGraphNodeHandle> diagnostics_cycle_nodes;
        if (should_rebuild_execution_plan)
        {
            plan = BuildExecutionPlan(plan_context);
            diagnostics_cycle_nodes = ApplyExecutionPlanResult(plan_context, plan, execution_cache_state);
            m_execution_plan_state.MarkPlanApplied();
        }

        if (should_update_dependency_diagnostics)
        {
            auto current_frame_resource_access = CollectFrameResourceAccessDiagnostics(nodes, m_render_graph_nodes);
            const unsigned cross_frame_comparison_window_size =
                GetCrossFrameComparisonWindowSize(frame_context.resource_frame_context);
            m_dependency_diagnostics_state.EnsureCrossFrameHazardSnapshotStorage(cross_frame_comparison_window_size);
            const unsigned hazard_slot_index =
                GetCrossFrameHazardSlotIndex(frame_context.resource_frame_context, cross_frame_comparison_window_size);

            ResourceAccessMaskMap previous_frame_resource_access_masks;
            ResourcePassAccessMap previous_frame_resource_pass_accesses;
            unsigned compared_previous_frame_count = 0u;
            const auto* previous_snapshot =
                m_dependency_diagnostics_state.GetCrossFrameHazardSnapshot(hazard_slot_index);
            const bool has_previous_frame_resource_access_masks = previous_snapshot != nullptr;
            if (previous_snapshot)
            {
                previous_frame_resource_access_masks = previous_snapshot->access_masks;
                previous_frame_resource_pass_accesses = previous_snapshot->pass_accesses;
                compared_previous_frame_count = 1u;
            }

            UpdateDependencyDiagnostics(
                plan_context,
                plan,
                execution_cache_state.cached_execution_graph_valid,
                execution_cache_state.cached_execution_signature,
                execution_cache_state.cached_execution_node_count,
                execution_cache_state.cached_execution_order.size(),
                std::move(diagnostics_cycle_nodes),
                m_auto_pruned_named_binding_counts,
                previous_frame_resource_access_masks,
                previous_frame_resource_pass_accesses,
                has_previous_frame_resource_access_masks,
                cross_frame_comparison_window_size,
                compared_previous_frame_count,
                current_frame_resource_access.access_masks,
                current_frame_resource_access.pass_accesses,
                m_dependency_diagnostics_state.diagnostics);
            {
                FrameResourceAccessSnapshot snapshot{};
                snapshot.access_masks = std::move(current_frame_resource_access.access_masks);
                snapshot.pass_accesses = std::move(current_frame_resource_access.pass_accesses);
                snapshot.frame_index = m_frame_index;
                m_dependency_diagnostics_state.UpdateCrossFrameHazardSnapshot(hazard_slot_index, std::move(snapshot));
            }

            m_dependency_diagnostics_state.MarkUpdated(m_frame_index);
        }

        const auto planning_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.execution_planning_ms = ToMilliseconds(planning_begin, planning_end);

        GLTF_CHECK(frame_context.command_list);
        ExecutePlanAndCollectStats(
            *frame_context.command_list,
            frame_context.resource_frame_context,
            frame_context.profiler_slot_index,
            interval);

        const auto collect_unused_begin = std::chrono::steady_clock::now();
        CollectUnusedRenderPassDescriptorResources(frame_context.resource_frame_context);
        const auto collect_unused_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.collect_unused_descriptor_ms =
            ToMilliseconds(collect_unused_begin, collect_unused_end);
    }

    void RenderGraph::BlitFinalOutputToSwapchain(
        IRHICommandList& command_list,
        const FrameContextSnapshot& frame_context,
        unsigned window_width,
        unsigned window_height)
    {
        if (!m_render_graph_node_handles.empty())
        {
            auto src = m_final_color_output;
            auto dst = m_resource_allocator.GetCurrentSwapchainRT(frame_context).m_source;

            src->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
            dst->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);

            const auto& src_desc = src->GetTextureDesc();
            const auto& dst_desc = dst->GetTextureDesc();
            const unsigned copy_width = (std::min)((std::min)(window_width, src_desc.GetTextureWidth()), dst_desc.GetTextureWidth());
            const unsigned copy_height = (std::min)((std::min)(window_height, src_desc.GetTextureHeight()), dst_desc.GetTextureHeight());
            if (copy_width > 0 && copy_height > 0)
            {
                RHICopyTextureInfo copy_info{};
                copy_info.copy_width = copy_width;
                copy_info.copy_height = copy_height;
                copy_info.dst_mip_level = 0;
                copy_info.src_mip_level = 0;
                copy_info.dst_x = 0;
                copy_info.dst_y = 0;
                RHIUtilInstanceManager::Instance().CopyTexture(command_list, *dst, *src, copy_info);
            }
        }
    }

    bool RenderGraph::AcquireCurrentFrameSwapchain(FramePreparationContext& frame_context)
    {
        const auto acquire_frame_begin = std::chrono::steady_clock::now();
        const bool acquire_succeeded = m_resource_allocator.GetCurrentSwapchain().AcquireNewFrame(m_resource_allocator.GetDevice());
        const auto acquire_frame_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.acquire_swapchain_ms = ToMilliseconds(acquire_frame_begin, acquire_frame_end);
        if (!acquire_succeeded)
        {
            m_resource_allocator.NotifySwapchainAcquireFailure();
            frame_context.presentation_frame_context = {};
            return false;
        }

        frame_context.presentation_frame_context = m_resource_allocator.GetFrameContext();
        return true;
    }

    void RenderGraph::FinalizeFrameSubmission(FramePreparationContext& frame_context, bool swapchain_ready)
    {
        GLTF_CHECK(frame_context.command_list);
        auto& command_list = *frame_context.command_list;
        if (!swapchain_ready)
        {
            const auto submit_begin = std::chrono::steady_clock::now();
            CloseCurrentCommandListAndExecute(command_list, {}, false);
            const auto submit_end = std::chrono::steady_clock::now();
            m_current_frame_timing_breakdown.submit_command_list_ms = ToMilliseconds(submit_begin, submit_end);
            FinalizeRenderDocFrameCapture();

            // Clear all nodes at end of frame
            m_render_graph_node_handles.clear();
            return;
        }

        const auto render_debug_ui_begin = std::chrono::steady_clock::now();
        GLTF_CHECK(RenderDebugUI(command_list, frame_context.presentation_frame_context));
        const auto render_debug_ui_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.render_debug_ui_ms = ToMilliseconds(render_debug_ui_begin, render_debug_ui_end);

        const auto present_begin = std::chrono::steady_clock::now();
        Present(command_list, frame_context.presentation_frame_context);
        const auto present_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.present_ms = ToMilliseconds(present_begin, present_end);
        FinalizeRenderDocFrameCapture();

        // Clear all nodes at end of frame
        m_render_graph_node_handles.clear();
    }

    void RenderGraph::RegisterTextureToColorOutput(TextureHandle texture_handle)
    {
        m_final_color_output_texture_handle = texture_handle;
        m_final_color_output_render_target_handle = NULL_HANDLE;
        auto texture = InternalResourceHandleTable::Instance().GetTexture(texture_handle);
        m_final_color_output = texture->m_texture;
        m_missing_final_color_output_logged = false;
    }

    void RenderGraph::RegisterRenderTargetToColorOutput(RenderTargetHandle render_target_handle)
    {
        m_final_color_output_render_target_handle = render_target_handle;
        m_final_color_output_texture_handle = NULL_HANDLE;
        auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_handle);
        m_final_color_output = render_target->m_source;
        m_missing_final_color_output_logged = false;
    }

    void RenderGraph::RegisterTickCallback(const RenderGraphTickCallback& callback)
    {
        m_tick_callback = callback;
    }

    void RenderGraph::RegisterDebugUICallback(const RenderGraphDebugUICallback& callback)
    {
        m_debug_ui_callback = callback;
    }

    void RenderGraph::EnableDebugUI(bool enable)
    {
        if (enable && !m_debug_ui_initialized)
        {
            GLTF_CHECK(InitDebugUI());
        }
        m_debug_ui_enabled = enable;
    }

    bool RenderGraph::ConfigureRenderDocCapture(bool enable, bool require_available, std::string& out_status)
    {
        if (!enable)
        {
            if (m_renderdoc_capture_state)
            {
                auto& renderdoc_state = *m_renderdoc_capture_state;
                if (renderdoc_state.api && renderdoc_state.api->MaskOverlayBits && renderdoc_state.overlay_hidden)
                {
                    const uint32_t overlay_bits = renderdoc_state.overlay_bits_saved
                        ? renderdoc_state.saved_overlay_bits
                        : static_cast<uint32_t>(eRENDERDOC_Overlay_None);
                    renderdoc_state.api->MaskOverlayBits(0u, overlay_bits);
                    renderdoc_state.overlay_hidden = false;
                }
                renderdoc_state.enabled = false;
                renderdoc_state.required = false;
                renderdoc_state.pending = {};
            }
            out_status = "RenderDoc capture disabled.";
            return true;
        }

        if (!m_renderdoc_capture_state)
        {
            m_renderdoc_capture_state = std::make_unique<RenderDocCaptureState>();
        }

        auto& renderdoc_state = *m_renderdoc_capture_state;
        renderdoc_state.enabled = true;
        renderdoc_state.required = require_available;
        if (renderdoc_state.available && renderdoc_state.api)
        {
            if (renderdoc_state.api->MaskOverlayBits)
            {
                if (!renderdoc_state.overlay_bits_saved && renderdoc_state.api->GetOverlayBits)
                {
                    renderdoc_state.saved_overlay_bits = renderdoc_state.api->GetOverlayBits();
                    renderdoc_state.overlay_bits_saved = true;
                }
                renderdoc_state.api->MaskOverlayBits(0u, 0u);
                renderdoc_state.overlay_hidden = true;
            }
            out_status = renderdoc_state.status.empty() ? "RenderDoc API ready." : renderdoc_state.status;
            return true;
        }

        const bool initialized = InitRenderDocCapture(require_available, out_status);
        if (!initialized && !require_available)
        {
            return true;
        }
        return initialized;
    }

    bool RenderGraph::RequestRenderDocCaptureForCurrentFrame(const std::filesystem::path& capture_path, std::string& out_error)
    {
        out_error.clear();
        if (!m_renderdoc_capture_state || !m_renderdoc_capture_state->enabled)
        {
            out_error = "RenderDoc capture is not enabled for this run.";
            return false;
        }

        auto& renderdoc_state = *m_renderdoc_capture_state;
        if (!renderdoc_state.available || !renderdoc_state.api)
        {
            out_error = !renderdoc_state.status.empty() ? renderdoc_state.status : "RenderDoc API unavailable.";
            return false;
        }
        if (renderdoc_state.pending.armed)
        {
            out_error = "RenderDoc capture is already armed for a frame.";
            return false;
        }
        if (renderdoc_state.api->IsFrameCapturing())
        {
            out_error = "RenderDoc is already capturing a frame.";
            return false;
        }

        std::error_code fs_error;
        const std::filesystem::path absolute_path = std::filesystem::absolute(capture_path, fs_error);
        const std::filesystem::path resolved_path = fs_error ? capture_path : absolute_path;
        const auto parent_path = resolved_path.parent_path();
        if (!parent_path.empty())
        {
            std::filesystem::create_directories(parent_path, fs_error);
            if (fs_error)
            {
                out_error = "Failed to create RenderDoc output directory: " + parent_path.string();
                return false;
            }
        }

        renderdoc_state.pending = {};
        renderdoc_state.pending.armed = true;
        renderdoc_state.pending.frame_index = m_frame_index;
        renderdoc_state.pending.capture_count_before = renderdoc_state.api->GetNumCaptures();
        renderdoc_state.pending.requested_path = resolved_path;
        return true;
    }

    bool RenderGraph::IsRenderDocCaptureEnabled() const
    {
        return m_renderdoc_capture_state && m_renderdoc_capture_state->enabled;
    }

    unsigned long long RenderGraph::GetCurrentFrameIndex() const
    {
        return m_frame_index;
    }

    bool RenderGraph::IsRenderDocCaptureAvailable() const
    {
        return m_renderdoc_capture_state && m_renderdoc_capture_state->available && m_renderdoc_capture_state->api;
    }

    bool RenderGraph::WasLastRenderDocCaptureSuccessful() const
    {
        return m_renderdoc_capture_state && m_renderdoc_capture_state->last_result.success;
    }

    unsigned long long RenderGraph::GetLastRenderDocCaptureFrameIndex() const
    {
        return m_renderdoc_capture_state ? m_renderdoc_capture_state->last_result.frame_index : 0ull;
    }

    std::string RenderGraph::GetLastRenderDocCapturePath() const
    {
        return m_renderdoc_capture_state ? m_renderdoc_capture_state->last_result.capture_path : std::string{};
    }

    std::string RenderGraph::GetLastRenderDocCaptureError() const
    {
        return m_renderdoc_capture_state ? m_renderdoc_capture_state->last_result.error : std::string{};
    }

    bool RenderGraph::OpenRenderDocCaptureInReplayUI(const std::filesystem::path& capture_path, std::string& out_status)
    {
        out_status.clear();

        std::error_code fs_error{};
        const std::filesystem::path absolute_path = std::filesystem::absolute(capture_path, fs_error);
        const std::filesystem::path resolved_path = fs_error ? capture_path : absolute_path;
        if (resolved_path.empty())
        {
            out_status = "RenderDoc replay launch failed: capture path is empty.";
            return false;
        }

        const bool capture_exists = std::filesystem::exists(resolved_path, fs_error) && !fs_error;
        if (!capture_exists)
        {
            out_status = "RenderDoc replay launch failed: capture file does not exist: " + resolved_path.string();
            return false;
        }

        if (m_renderdoc_capture_state && m_renderdoc_capture_state->available && m_renderdoc_capture_state->api)
        {
            auto& renderdoc_state = *m_renderdoc_capture_state;
            if (renderdoc_state.api->LaunchReplayUI)
            {
                const std::string command_line = QuoteCommandLineArgument(PathToUtf8String(resolved_path));
                const uint32_t replay_pid = renderdoc_state.api->LaunchReplayUI(1u, command_line.c_str());
                if (replay_pid != 0u)
                {
                    out_status =
                        "RenderDoc replay UI launched for capture: " + resolved_path.string() +
                        " (pid " + std::to_string(replay_pid) + ").";
                    return true;
                }
            }
        }

        const auto shell_result = reinterpret_cast<intptr_t>(
            ShellExecuteW(nullptr, L"open", resolved_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (shell_result > 32)
        {
            out_status = "RenderDoc capture opened via shell association: " + resolved_path.string();
            return true;
        }

        if (m_renderdoc_capture_state && m_renderdoc_capture_state->available && m_renderdoc_capture_state->api)
        {
            auto& renderdoc_state = *m_renderdoc_capture_state;
            if (renderdoc_state.api->ShowReplayUI &&
                renderdoc_state.api->IsTargetControlConnected &&
                renderdoc_state.api->IsTargetControlConnected() != 0 &&
                renderdoc_state.api->ShowReplayUI() != 0)
            {
                out_status =
                    "RenderDoc replay UI was shown, but the capture file could not be opened automatically: " +
                    resolved_path.string();
                return true;
            }
        }

        out_status = "RenderDoc replay launch failed for capture: " + resolved_path.string();
        return false;
    }

    bool RenderGraph::OpenLastRenderDocCaptureInReplayUI(std::string& out_status)
    {
        const std::string capture_path = GetLastRenderDocCapturePath();
        if (capture_path.empty())
        {
            out_status = "RenderDoc replay launch failed: there is no completed capture to open.";
            return false;
        }
        return OpenRenderDocCaptureInReplayUI(std::filesystem::path(capture_path), out_status);
    }

    void RenderGraph::ShutdownRuntimeServices()
    {
        m_tick_callback = nullptr;
        m_debug_ui_callback = nullptr;
        m_final_color_output.reset();
        m_final_color_output_texture_handle = NULL_HANDLE;
        m_final_color_output_render_target_handle = NULL_HANDLE;
        m_missing_final_color_output_logged = false;
        ShutdownRenderDocCapture();
        ShutdownGPUProfiler();
        ShutdownDebugUI();
    }

    void RenderGraph::SetValidationPolicy(const ValidationPolicy& policy)
    {
        m_validation_policy = policy;
        m_validation_policy.log_interval_frames = (std::max)(1u, m_validation_policy.log_interval_frames);
        m_validation_policy.cross_frame_hazard_check_interval_frames =
            (std::max)(1u, m_validation_policy.cross_frame_hazard_check_interval_frames);
        m_dependency_diagnostics_state.Reset();
    }

    RenderGraph::ValidationPolicy RenderGraph::GetValidationPolicy() const
    {
        return m_validation_policy;
    }

    const RenderGraph::FrameStats& RenderGraph::GetLastFrameStats() const
    {
        return m_last_frame_stats;
    }

    const RenderGraph::FrameTimingBreakdown& RenderGraph::GetLastFrameTimingBreakdown() const
    {
        return m_last_frame_timing_breakdown;
    }

    void RenderGraph::SetTickCallbackBreakdown(float other_ms, float module_ms, float system_ms)
    {
        m_current_frame_timing_breakdown.tick_other_ms = (std::max)(0.0f, other_ms);
        m_current_frame_timing_breakdown.module_tick_ms = (std::max)(0.0f, module_ms);
        m_current_frame_timing_breakdown.system_tick_ms = (std::max)(0.0f, system_ms);
    }

    const RenderGraph::DependencyDiagnostics& RenderGraph::GetDependencyDiagnostics() const
    {
        return m_dependency_diagnostics_state.diagnostics;
    }

    void RenderGraph::DrawFrameworkDebugUI()
    {
        bool per_frame_resource_binding = m_resource_allocator.IsPerFrameResourceBindingEnabled();
        ImGui::TextUnformatted("Resource Binding");
        if (ImGui::Checkbox("Per-frame Resource Binding", &per_frame_resource_binding))
        {
            m_resource_allocator.SetPerFrameResourceBindingEnabled(per_frame_resource_binding);
            m_dependency_diagnostics_state.Reset();
        }
        ImGui::TextUnformatted("Affects frame-buffered Buffer/RenderTarget handle selection.");

        ImGui::Separator();
        ImGui::TextUnformatted("Cross-frame Hazard Analysis");
        int hazard_check_interval_frames =
            static_cast<int>(m_validation_policy.cross_frame_hazard_check_interval_frames);
        if (ImGui::SliderInt("Hazard Analysis Interval (frames)", &hazard_check_interval_frames, 1, 240))
        {
            m_validation_policy.cross_frame_hazard_check_interval_frames =
                static_cast<unsigned>((std::max)(1, hazard_check_interval_frames));
            m_dependency_diagnostics_state.Reset();
        }

        const auto& dependency_diagnostics = m_dependency_diagnostics_state.diagnostics;
        const ImGuiTableFlags summary_table_flags =
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("FrameworkSummaryTable", 2, summary_table_flags))
        {
            ImGui::TableSetupColumn("Metric");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Binding Mode");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(per_frame_resource_binding ? "Per-frame" : "Shared");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Hazard Cadence");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u frame(s)", m_validation_policy.cross_frame_hazard_check_interval_frames);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Analysis State");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(
                dependency_diagnostics.cross_frame_analysis_ready
                    ? "Ready"
                    : "Warming up (need previous frame)");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Recorded Hazards");
            ImGui::TableSetColumnIndex(1);
            if (dependency_diagnostics.cross_frame_analysis_ready)
            {
                ImGui::Text(
                    "%u (window=%u, compared=%u)",
                    dependency_diagnostics.cross_frame_hazard_count,
                    dependency_diagnostics.cross_frame_comparison_window_size,
                    dependency_diagnostics.cross_frame_compared_frame_count);
            }
            else
            {
                ImGui::TextUnformatted("-");
            }

            if (dependency_diagnostics.cross_frame_hazard_overflow_count > 0)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Unlisted Hazards");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", dependency_diagnostics.cross_frame_hazard_overflow_count);
            }

            ImGui::EndTable();
        }

        if (!dependency_diagnostics.cross_frame_analysis_ready)
        {
            return;
        }

        if (!dependency_diagnostics.cross_frame_hazards.empty() &&
            ImGui::TreeNode("Cross-frame Hazard Samples"))
        {
            const ImGuiTableFlags hazard_table_flags =
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("CrossFrameHazardTable", 4, hazard_table_flags, ImVec2(0.0f, 180.0f)))
            {
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Resource");
                ImGui::TableSetupColumn("Previous Passes");
                ImGui::TableSetupColumn("Current Passes");
                ImGui::TableHeadersRow();

                for (const auto& hazard : dependency_diagnostics.cross_frame_hazards)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(ToCrossFrameHazardTypeName(hazard.hazard_type));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s:%u", hazard.resource_kind.c_str(), hazard.resource_id);
                    ImGui::TableSetColumnIndex(2);
                    if (!hazard.previous_passes.empty())
                    {
                        const std::string previous_passes_text = JoinPassNames(hazard.previous_passes);
                        ImGui::TextWrapped("%s", previous_passes_text.c_str());
                    }
                    else
                    {
                        ImGui::TextUnformatted("-");
                    }
                    ImGui::TableSetColumnIndex(3);
                    if (!hazard.current_passes.empty())
                    {
                        const std::string current_passes_text = JoinPassNames(hazard.current_passes);
                        ImGui::TextWrapped("%s", current_passes_text.c_str());
                    }
                    else
                    {
                        ImGui::TextUnformatted("-");
                    }
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    bool CleanupRenderRuntimeContext(
        std::shared_ptr<RenderGraph>& render_graph,
        std::shared_ptr<ResourceOperator>& resource_operator,
        bool clear_window_handles)
    {
        if (resource_operator)
        {
            resource_operator->WaitFrameRenderFinished();
            resource_operator->WaitGPUIdle();
        }

        if (render_graph)
        {
            render_graph->ShutdownRuntimeServices();
            render_graph.reset();
        }

        if (!resource_operator)
        {
            return true;
        }

        const bool cleanup_result = resource_operator->CleanupAllResources(clear_window_handles);
        resource_operator.reset();
        return cleanup_result;
    }

    bool PreloadRenderDocRuntime(RenderDeviceType device_type, bool require_available, std::string& out_status)
    {
        out_status.clear();

        HMODULE module_handle = GetModuleHandleA("renderdoc.dll");
        const bool module_loaded_by_app = g_renderdoc_runtime_loaded_by_app;
        if (device_type == VULKAN)
        {
            const std::filesystem::path manifest_path = QueryRenderDocVulkanManifestPath();
            int implementation_version = 0;
            const bool has_supported_manifest =
                QueryRenderDocVulkanImplementationVersion(manifest_path, implementation_version) &&
                implementation_version >= 43;

            if (has_supported_manifest)
            {
                SetEnvironmentVariableW(L"ENABLE_VULKAN_RENDERDOC_CAPTURE", L"1");
                SetEnvironmentVariableW(L"DISABLE_VULKAN_RENDERDOC_CAPTURE_1_13", nullptr);
                out_status =
                    "RenderDoc Vulkan implicit layer enabled before device initialization (implementation " +
                    std::to_string(implementation_version) + ").";
                return true;
            }

            if (module_handle && !module_loaded_by_app)
            {
                out_status =
                    "RenderDoc runtime is already present before Vulkan device initialization, "
                    "but no registered RenderDoc 1.43+ implicit-layer manifest was found. "
                    "Continuing under the assumption that RenderDoc was injected externally.";
                return true;
            }

            out_status =
                "RenderDoc runtime must already be injected before Vulkan device initialization. "
                "Automatic Vulkan implicit-layer activation requires a registered RenderDoc 1.43+ installation.";
            return false;
        }

        if (!module_handle)
        {
            module_handle = LoadLibraryA("renderdoc.dll");
            if (module_handle)
            {
                g_renderdoc_runtime_loaded_by_app = true;
            }
        }
        if (!module_handle)
        {
            const std::filesystem::path installed_module_path = QueryInstalledRenderDocModulePath();
            if (!installed_module_path.empty())
            {
                module_handle = LoadLibraryW(installed_module_path.c_str());
                if (module_handle)
                {
                    g_renderdoc_runtime_loaded_by_app = true;
                    out_status = "RenderDoc runtime preloaded before device initialization from registered installation.";
                }
            }
        }

        if (!module_handle)
        {
            out_status = "RenderDoc runtime unavailable before device initialization: renderdoc.dll was not found.";
            return false;
        }

        const auto get_api = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(module_handle, "RENDERDOC_GetAPI"));
        if (!get_api)
        {
            out_status = "RenderDoc runtime unavailable before device initialization: RENDERDOC_GetAPI entry point was not found.";
            return false;
        }

        (void)require_available;
        out_status = "RenderDoc runtime preloaded before device initialization.";
        return true;
    }

    void ShutdownWindowing()
    {
        glTFWindow::Get().ShutdownGLFW();
    }

    bool RenderGraph::InitDebugUI()
    {
        if (m_debug_ui_initialized)
        {
            return true;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        GLTF_CHECK(ImGui_ImplGlfw_InitForOther(glTFWindow::Get().GetGLFWWindow(), true));
        GLTF_CHECK(RHIUtilInstanceManager::Instance().InitGUIContext(
            m_resource_allocator.GetDevice(),
            m_resource_allocator.GetCommandQueue(),
            m_resource_allocator.GetDescriptorManager(),
            ResolveSwapchainImageCount(m_resource_allocator)));

        glTFWindow::Get().SetInputHandleCallback([this]()
        {
            if (!m_debug_ui_enabled || !m_debug_ui_initialized)
            {
                return false;
            }
            const ImGuiIO& io = ImGui::GetIO();
            return io.WantCaptureMouse || io.WantCaptureKeyboard;
        });
        m_debug_ui_initialized = true;
        return true;
    }

    bool RenderGraph::RenderDebugUI(IRHICommandList& command_list, const FrameContextSnapshot& frame_context)
    {
        if (!m_debug_ui_enabled || !m_debug_ui_initialized)
        {
            return true;
        }
        if (!m_resource_allocator.HasCurrentSwapchainRT())
        {
            return true;
        }

        ImGui::Render();

        RHIBeginRenderingInfo begin_rendering_info{};
        auto& swapchain_rt = m_resource_allocator.GetCurrentSwapchainRT(frame_context);
        const auto& swapchain_desc = swapchain_rt.m_source->GetTextureDesc();
        begin_rendering_info.rendering_area_width = swapchain_desc.GetTextureWidth();
        begin_rendering_info.rendering_area_height = swapchain_desc.GetTextureHeight();
        begin_rendering_info.enable_depth_write = false;
        begin_rendering_info.clear_render_target = false;
        begin_rendering_info.m_render_targets = {&swapchain_rt};

        GLTF_CHECK(RHIUtilInstanceManager::Instance().BeginRendering(command_list, begin_rendering_info));
        GLTF_CHECK(m_resource_allocator.GetDescriptorManager().BindGUIDescriptorContext(command_list));
        GLTF_CHECK(RHIUtilInstanceManager::Instance().RenderGUIFrame(command_list));
        GLTF_CHECK(RHIUtilInstanceManager::Instance().EndRendering(command_list));

        return true;
    }

    void RenderGraph::ShutdownDebugUI()
    {
        if (!m_debug_ui_initialized)
        {
            return;
        }

        m_debug_ui_callback = nullptr;
        m_debug_ui_enabled = false;
        glTFWindow::Get().SetInputHandleCallback([]() { return false; });
        RHIUtilInstanceManager::Instance().ExitGUI();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_debug_ui_initialized = false;
    }

    bool RenderGraph::InitRenderDocCapture(bool require_available, std::string& out_status)
    {
        if (!m_renderdoc_capture_state)
        {
            m_renderdoc_capture_state = std::make_unique<RenderDocCaptureState>();
        }

        auto& renderdoc_state = *m_renderdoc_capture_state;
        renderdoc_state.required = require_available;
        if (renderdoc_state.available && renderdoc_state.api)
        {
            out_status = renderdoc_state.status;
            return true;
        }

        HMODULE module_handle = GetModuleHandleA("renderdoc.dll");
        bool owns_module = false;
        if (!module_handle)
        {
            if (RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan)
            {
                renderdoc_state.available = false;
                renderdoc_state.status =
                    "RenderDoc API unavailable: Vulkan capture requires RenderDoc to be injected before device initialization.";
                out_status = renderdoc_state.status;
                return false;
            }

            module_handle = LoadLibraryA("renderdoc.dll");
            owns_module = module_handle != nullptr;
            if (owns_module)
            {
                g_renderdoc_runtime_loaded_by_app = true;
            }
            if (!module_handle)
            {
                const std::filesystem::path installed_module_path = QueryInstalledRenderDocModulePath();
                if (!installed_module_path.empty())
                {
                    module_handle = LoadLibraryW(installed_module_path.c_str());
                    owns_module = module_handle != nullptr;
                    if (owns_module)
                    {
                        g_renderdoc_runtime_loaded_by_app = true;
                    }
                }
            }
        }
        if (!module_handle)
        {
            renderdoc_state.available = false;
            renderdoc_state.status =
                "RenderDoc API unavailable: renderdoc.dll was not found in the process or DLL search path.";
            out_status = renderdoc_state.status;
            return false;
        }

        const auto get_api = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(module_handle, "RENDERDOC_GetAPI"));
        if (!get_api)
        {
            if (owns_module)
            {
                FreeLibrary(module_handle);
            }
            renderdoc_state.available = false;
            renderdoc_state.status = "RenderDoc API unavailable: RENDERDOC_GetAPI entry point was not found.";
            out_status = renderdoc_state.status;
            return false;
        }

        struct RenderDocApiVersionCandidate
        {
            RENDERDOC_Version version;
            int major;
            int minor;
            int patch;
        };

        constexpr RenderDocApiVersionCandidate k_renderdoc_version_candidates[] = {
            {eRENDERDOC_API_Version_1_7_0, 1, 7, 0},
            {eRENDERDOC_API_Version_1_6_0, 1, 6, 0},
            {eRENDERDOC_API_Version_1_5_0, 1, 5, 0},
            {eRENDERDOC_API_Version_1_4_2, 1, 4, 2},
            {eRENDERDOC_API_Version_1_4_1, 1, 4, 1},
            {eRENDERDOC_API_Version_1_4_0, 1, 4, 0},
            {eRENDERDOC_API_Version_1_3_0, 1, 3, 0},
        };

        RENDERDOC_API_1_7_0* renderdoc_api = nullptr;
        const RenderDocApiVersionCandidate* negotiated_version = nullptr;
        for (const auto& version_candidate : k_renderdoc_version_candidates)
        {
            renderdoc_api = nullptr;
            if (get_api(version_candidate.version, reinterpret_cast<void**>(&renderdoc_api)) != 0 && renderdoc_api)
            {
                negotiated_version = &version_candidate;
                break;
            }
        }

        if (!renderdoc_api || !negotiated_version)
        {
            if (owns_module)
            {
                FreeLibrary(module_handle);
            }
            renderdoc_state.available = false;
            renderdoc_state.status =
                "RenderDoc API unavailable: supported API version is older than 1.3.0.";
            out_status = renderdoc_state.status;
            return false;
        }

        renderdoc_state.available = true;
        renderdoc_state.owns_module = owns_module;
        renderdoc_state.module_handle = module_handle;
        renderdoc_state.api = renderdoc_api;
        int major = 0;
        int minor = 0;
        int patch = 0;
        renderdoc_api->GetAPIVersion(&major, &minor, &patch);
        renderdoc_state.api_version = major * 10000 + minor * 100 + patch;
        if (renderdoc_api->MaskOverlayBits)
        {
            if (!renderdoc_state.overlay_bits_saved && renderdoc_api->GetOverlayBits)
            {
                renderdoc_state.saved_overlay_bits = renderdoc_api->GetOverlayBits();
                renderdoc_state.overlay_bits_saved = true;
            }
            renderdoc_api->MaskOverlayBits(0u, 0u);
            renderdoc_state.overlay_hidden = true;
        }
        renderdoc_state.status = "RenderDoc API ready (" +
            std::to_string(major) + "." +
            std::to_string(minor) + "." +
            std::to_string(patch) + ", negotiated " +
            std::to_string(negotiated_version->major) + "." +
            std::to_string(negotiated_version->minor) + "." +
            std::to_string(negotiated_version->patch) + ").";
        out_status = renderdoc_state.status;
        return true;
    }

    void RenderGraph::BeginRenderDocFrameCapture()
    {
        if (!m_renderdoc_capture_state)
        {
            return;
        }

        auto& renderdoc_state = *m_renderdoc_capture_state;
        if (!renderdoc_state.enabled ||
            !renderdoc_state.available ||
            !renderdoc_state.api ||
            !renderdoc_state.pending.armed ||
            renderdoc_state.pending.started ||
            renderdoc_state.pending.frame_index != m_frame_index)
        {
            return;
        }

        renderdoc_state.last_result = {};
        renderdoc_state.last_result.attempted = true;
        renderdoc_state.last_result.frame_index = renderdoc_state.pending.frame_index;

        const std::string capture_template = BuildRenderDocCaptureTemplate(renderdoc_state.pending.requested_path);
        if (capture_template.empty())
        {
            renderdoc_state.last_result.error = "Failed to build RenderDoc capture file template.";
            renderdoc_state.pending = {};
            return;
        }

        renderdoc_state.api->SetCaptureFilePathTemplate(capture_template.c_str());
        const auto window_handle = reinterpret_cast<RENDERDOC_WindowHandle>(m_window.GetHWND());
        renderdoc_state.api->StartFrameCapture(nullptr, window_handle);
        if (!renderdoc_state.api->IsFrameCapturing())
        {
            renderdoc_state.last_result.error = "RenderDoc failed to start frame capture.";
            renderdoc_state.pending = {};
            return;
        }

        const std::string capture_title = PathToUtf8String(renderdoc_state.pending.requested_path.stem());
        if (!capture_title.empty() &&
            renderdoc_state.api_version >= static_cast<int>(eRENDERDOC_API_Version_1_6_0) &&
            renderdoc_state.api->SetCaptureTitle)
        {
            renderdoc_state.api->SetCaptureTitle(capture_title.c_str());
        }

        renderdoc_state.pending.started = true;
    }

    void RenderGraph::FinalizeRenderDocFrameCapture()
    {
        if (!m_renderdoc_capture_state)
        {
            return;
        }

        auto& renderdoc_state = *m_renderdoc_capture_state;
        if (!renderdoc_state.pending.armed || renderdoc_state.pending.frame_index != m_frame_index)
        {
            return;
        }

        auto& result = renderdoc_state.last_result;
        if (!result.attempted)
        {
            result = {};
            result.attempted = true;
            result.frame_index = renderdoc_state.pending.frame_index;
        }

        if (!renderdoc_state.pending.started || !renderdoc_state.api)
        {
            if (result.error.empty())
            {
                result.error = "RenderDoc capture was requested but did not start.";
            }
            renderdoc_state.pending = {};
            return;
        }

        const auto window_handle = reinterpret_cast<RENDERDOC_WindowHandle>(m_window.GetHWND());
        if (renderdoc_state.api->EndFrameCapture(nullptr, window_handle) == 0)
        {
            result.success = false;
            if (result.error.empty())
            {
                result.error = "RenderDoc failed to finalize frame capture.";
            }
            renderdoc_state.pending = {};
            return;
        }

        const uint32_t capture_count_after = renderdoc_state.api->GetNumCaptures();
        if (capture_count_after <= renderdoc_state.pending.capture_count_before)
        {
            result.success = false;
            result.error = "RenderDoc reported success but did not register a new capture file.";
            renderdoc_state.pending = {};
            return;
        }

        result.capture_path = QueryRenderDocCapturePath(renderdoc_state.api, capture_count_after - 1u);
        result.success = !result.capture_path.empty();
        if (!result.success)
        {
            result.error = "RenderDoc captured the frame but the output path could not be queried.";
        }
        else
        {
            result.error.clear();
        }

        renderdoc_state.pending = {};
    }

    void RenderGraph::ShutdownRenderDocCapture()
    {
        if (!m_renderdoc_capture_state)
        {
            return;
        }

        auto& renderdoc_state = *m_renderdoc_capture_state;
        if (renderdoc_state.api && renderdoc_state.api->MaskOverlayBits && renderdoc_state.overlay_hidden)
        {
            const uint32_t overlay_bits = renderdoc_state.overlay_bits_saved
                ? renderdoc_state.saved_overlay_bits
                : static_cast<uint32_t>(eRENDERDOC_Overlay_None);
            renderdoc_state.api->MaskOverlayBits(0u, overlay_bits);
            renderdoc_state.overlay_hidden = false;
        }
        if (renderdoc_state.api && renderdoc_state.api->IsFrameCapturing())
        {
            const auto window_handle = reinterpret_cast<RENDERDOC_WindowHandle>(m_window.GetHWND());
            renderdoc_state.api->EndFrameCapture(nullptr, window_handle);
        }

        renderdoc_state.pending = {};
        renderdoc_state.api = nullptr;
        if (renderdoc_state.module_handle && renderdoc_state.owns_module)
        {
            FreeLibrary(renderdoc_state.module_handle);
        }
        renderdoc_state.module_handle = nullptr;
        renderdoc_state.owns_module = false;
        m_renderdoc_capture_state.reset();
    }

    bool RenderGraph::InitGPUProfiler()
    {
        if (m_gpu_profiler_state)
        {
            return true;
        }

        m_gpu_profiler_state = std::make_unique<GPUProfilerState>();
        auto& profiler_state = *m_gpu_profiler_state;
        const unsigned profiler_slot_count = ResolveProfilerSlotCount(m_resource_allocator);
        if (profiler_slot_count == 0)
        {
            return true;
        }

        profiler_state.frame_slots.resize(profiler_slot_count);
        
        GLTF_CHECK(RHIUtilInstanceManager::Instance().InitTimestampProfiler(
            m_resource_allocator.GetDevice(),
            m_resource_allocator.GetCommandQueue(),
            profiler_slot_count,
            profiler_state.max_query_count));
        profiler_state.supported = RHIUtilInstanceManager::Instance().IsTimestampProfilerSupported();
        if (profiler_state.supported)
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Profiler] GPU timestamp profiler enabled.\n");
        }
        else
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Profiler] GPU timestamp profiler unavailable, fallback to CPU timings only.\n");
        }

        return true;
    }

    void RenderGraph::ShutdownGPUProfiler()
    {
        if (!m_gpu_profiler_state)
        {
            return;
        }

        RHIUtilInstanceManager::Instance().ShutdownTimestampProfiler();
        m_gpu_profiler_state.reset();
    }

    bool RenderGraph::HasValidGPUProfilerSlot(unsigned slot_index) const
    {
        return m_gpu_profiler_state &&
            m_gpu_profiler_state->supported &&
            slot_index < m_gpu_profiler_state->frame_slots.size();
    }

    unsigned RenderGraph::GetGPUProfilerMaxTimestampedPassCount() const
    {
        return (m_gpu_profiler_state && m_gpu_profiler_state->supported)
            ? m_gpu_profiler_state->max_pass_count
            : 0u;
    }

    unsigned RenderGraph::GetGPUProfilerMaxQueryCount() const
    {
        return m_gpu_profiler_state ? m_gpu_profiler_state->max_query_count : 0u;
    }

    unsigned RenderGraph::ClampGPUProfilerQueryCount(unsigned query_count) const
    {
        const unsigned max_query_count = GetGPUProfilerMaxQueryCount();
        if (max_query_count == 0u)
        {
            return 0u;
        }

        return (std::min)(query_count, max_query_count);
    }

    void RenderGraph::ResolveGPUProfilerFrame(unsigned slot_index)
    {
        if (!HasValidGPUProfilerSlot(slot_index))
        {
            return;
        }

        auto& profiler_state = *m_gpu_profiler_state;
        auto& frame_slot = profiler_state.frame_slots[slot_index];
        if (!frame_slot.has_pending_frame_stats || frame_slot.query_count == 0)
        {
            return;
        }

        const unsigned query_count = frame_slot.query_count;
        std::vector<uint64_t> timestamps(query_count, 0);
        double ticks_per_second = 0.0;
        const bool readback_ok = RHIUtilInstanceManager::Instance().ResolveTimestampFrame(
            slot_index,
            query_count,
            timestamps,
            ticks_per_second);

        if (!readback_ok)
        {
            frame_slot.has_pending_frame_stats = false;
            frame_slot.query_count = 0;
            return;
        }

        auto resolved_stats = frame_slot.pending_frame_stats;
        resolved_stats.gpu_time_valid = false;
        resolved_stats.gpu_total_ms = 0.0f;

        const unsigned timed_pass_count = (std::min)(static_cast<unsigned>(resolved_stats.pass_stats.size()), query_count / 2);
        for (unsigned i = 0; i < timed_pass_count; ++i)
        {
            const uint64_t begin_tick = timestamps[i * 2];
            const uint64_t end_tick = timestamps[i * 2 + 1];
            if (end_tick <= begin_tick)
            {
                continue;
            }

            if (ticks_per_second <= 0.0)
            {
                break;
            }

            const double gpu_time_ms = static_cast<double>(end_tick - begin_tick) * 1000.0 / ticks_per_second;

            auto& pass_stats = resolved_stats.pass_stats[i];
            pass_stats.gpu_time_valid = true;
            pass_stats.gpu_time_ms = static_cast<float>(gpu_time_ms);
            resolved_stats.gpu_total_ms += static_cast<float>(gpu_time_ms);
            resolved_stats.gpu_time_valid = true;
        }

        m_last_frame_stats = resolved_stats;
        frame_slot.has_pending_frame_stats = false;
        frame_slot.query_count = 0;
    }

    void RenderGraph::ExecutePlanAndCollectStats(
        IRHICommandList& command_list,
        const FrameContextSnapshot& frame_context,
        unsigned profiler_slot_index,
        unsigned long long interval)
    {
        FrameStats submitted_frame_stats{};
        submitted_frame_stats.frame_index = m_frame_index;
        submitted_frame_stats.cpu_total_ms = 0.0f;
        submitted_frame_stats.cpu_executed_ms = 0.0f;
        submitted_frame_stats.cpu_skipped_ms = 0.0f;
        submitted_frame_stats.cpu_skipped_validation_ms = 0.0f;
        submitted_frame_stats.gpu_time_valid = false;
        submitted_frame_stats.gpu_total_ms = 0.0f;
        submitted_frame_stats.total_pass_count = 0;
        submitted_frame_stats.executed_pass_count = 0;
        submitted_frame_stats.skipped_pass_count = 0;
        submitted_frame_stats.skipped_validation_pass_count = 0;
        submitted_frame_stats.skipped_missing_pass_count = 0;
        submitted_frame_stats.graphics_pass_count = 0;
        submitted_frame_stats.compute_pass_count = 0;
        submitted_frame_stats.ray_tracing_pass_count = 0;
        submitted_frame_stats.executed_graphics_pass_count = 0;
        submitted_frame_stats.executed_compute_pass_count = 0;
        submitted_frame_stats.executed_ray_tracing_pass_count = 0;
        submitted_frame_stats.pass_stats.clear();
        submitted_frame_stats.pass_stats.reserve(m_execution_plan_state.cached_execution_order.size());

        GLTF_CHECK(BeginGPUProfilerFrame(command_list, profiler_slot_index));
        const unsigned max_timestamped_pass_count = GetGPUProfilerMaxTimestampedPassCount();
        unsigned timestamped_pass_count = 0;

        for (unsigned pass_index = 0; pass_index < m_execution_plan_state.cached_execution_order.size(); ++pass_index)
        {
            auto render_graph_node = m_execution_plan_state.cached_execution_order[pass_index];
            const bool enable_gpu_timestamp = max_timestamped_pass_count > 0 && pass_index < max_timestamped_pass_count;
            if (enable_gpu_timestamp)
            {
                const unsigned query_index = static_cast<unsigned>(pass_index * 2);
                GLTF_CHECK(WriteGPUProfilerTimestamp(command_list, profiler_slot_index, query_index));
            }

            const auto pass_begin = std::chrono::steady_clock::now();
            const auto execution_status =
                ExecuteRenderGraphNode(command_list, frame_context, render_graph_node, interval);
            const auto pass_end = std::chrono::steady_clock::now();
            const float pass_cpu_ms = std::chrono::duration<float, std::milli>(pass_end - pass_begin).count();

            if (enable_gpu_timestamp)
            {
                const unsigned query_index = static_cast<unsigned>(pass_index * 2 + 1);
                GLTF_CHECK(WriteGPUProfilerTimestamp(command_list, profiler_slot_index, query_index));
                ++timestamped_pass_count;
            }

            const auto& node_desc = m_render_graph_nodes[render_graph_node.value];
            std::string group_name = node_desc.debug_group;
            if (group_name.empty())
            {
                group_name = "Ungrouped";
            }

            const auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(node_desc.render_pass_handle);
            const RenderPassType pass_type = render_pass ? render_pass->GetRenderPassType() : RenderPassType::GRAPHICS;
            std::string pass_name = node_desc.debug_name;
            if (pass_name.empty())
            {
                std::string type_name = render_pass ? ToRenderPassTypeName(pass_type) : "Unknown";
                pass_name = type_name + "#" + std::to_string(render_graph_node.value);
            }

            RenderPassFrameStats pass_stats{};
            pass_stats.node_handle = render_graph_node;
            pass_stats.group_name = group_name;
            pass_stats.pass_name = pass_name;
            pass_stats.pass_type = pass_type;
            pass_stats.executed = execution_status == RenderPassExecutionStatus::EXECUTED;
            pass_stats.skipped_due_to_validation = execution_status == RenderPassExecutionStatus::SKIPPED_INVALID_DRAW_DESC;
            pass_stats.cpu_time_ms = pass_cpu_ms;
            submitted_frame_stats.pass_stats.push_back(pass_stats);
            ++submitted_frame_stats.total_pass_count;
            submitted_frame_stats.cpu_total_ms += pass_cpu_ms;
            switch (pass_type)
            {
            case RenderPassType::GRAPHICS:
                ++submitted_frame_stats.graphics_pass_count;
                break;
            case RenderPassType::COMPUTE:
                ++submitted_frame_stats.compute_pass_count;
                break;
            case RenderPassType::RAY_TRACING:
                ++submitted_frame_stats.ray_tracing_pass_count;
                break;
            }
            if (pass_stats.executed)
            {
                ++submitted_frame_stats.executed_pass_count;
                submitted_frame_stats.cpu_executed_ms += pass_cpu_ms;
                switch (pass_type)
                {
                case RenderPassType::GRAPHICS:
                    ++submitted_frame_stats.executed_graphics_pass_count;
                    break;
                case RenderPassType::COMPUTE:
                    ++submitted_frame_stats.executed_compute_pass_count;
                    break;
                case RenderPassType::RAY_TRACING:
                    ++submitted_frame_stats.executed_ray_tracing_pass_count;
                    break;
                }
            }
            else
            {
                ++submitted_frame_stats.skipped_pass_count;
                submitted_frame_stats.cpu_skipped_ms += pass_cpu_ms;
                if (pass_stats.skipped_due_to_validation)
                {
                    ++submitted_frame_stats.skipped_validation_pass_count;
                    submitted_frame_stats.cpu_skipped_validation_ms += pass_cpu_ms;
                }
                else
                {
                    ++submitted_frame_stats.skipped_missing_pass_count;
                }
            }
        }

        m_current_frame_timing_breakdown.execute_passes_ms = submitted_frame_stats.cpu_total_ms;
        GLTF_CHECK(FinalizeGPUProfilerFrame(command_list, profiler_slot_index, timestamped_pass_count * 2, submitted_frame_stats));
        if (!(m_gpu_profiler_state && m_gpu_profiler_state->supported))
        {
            m_last_frame_stats = submitted_frame_stats;
        }
        else if (timestamped_pass_count == 0)
        {
            m_last_frame_stats = submitted_frame_stats;
        }
        else if (m_last_frame_stats.pass_stats.empty())
        {
            m_last_frame_stats = submitted_frame_stats;
        }
    }

    bool RenderGraph::BeginGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index)
    {
        if (!HasValidGPUProfilerSlot(slot_index))
        {
            return true;
        }

        GLTF_CHECK(RHIUtilInstanceManager::Instance().BeginTimestampFrame(command_list, slot_index));
        return true;
    }

    bool RenderGraph::WriteGPUProfilerTimestamp(IRHICommandList& command_list, unsigned slot_index, unsigned query_index)
    {
        if (!HasValidGPUProfilerSlot(slot_index))
        {
            return true;
        }
        if (query_index >= GetGPUProfilerMaxQueryCount())
        {
            return true;
        }

        GLTF_CHECK(RHIUtilInstanceManager::Instance().WriteTimestamp(command_list, slot_index, query_index));
        return true;
    }

    bool RenderGraph::FinalizeGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index, unsigned query_count, const FrameStats& frame_stats)
    {
        if (!HasValidGPUProfilerSlot(slot_index))
        {
            return true;
        }

        auto& profiler_state = *m_gpu_profiler_state;
        auto& frame_slot = profiler_state.frame_slots[slot_index];
        const unsigned clamped_query_count = ClampGPUProfilerQueryCount(query_count);
        GLTF_CHECK(RHIUtilInstanceManager::Instance().EndTimestampFrame(command_list, slot_index, clamped_query_count));

        frame_slot.query_count = clamped_query_count;
        frame_slot.pending_frame_stats = frame_stats;
        frame_slot.has_pending_frame_stats = clamped_query_count > 0;
        return true;
    }

    void RenderGraph::EnqueueResourceForDeferredRelease(
        const std::shared_ptr<IRHIResource>& resource,
        const FrameContextSnapshot& frame_context)
    {
        if (!resource)
        {
            return;
        }

        const unsigned delay_frame = ResolveDeferredReleaseLatencyFrames(frame_context);
        m_deferred_release_queue.Enqueue(resource, m_frame_index, delay_frame);
    }

    void RenderGraph::EnqueueRetainedObjectForDeferredRelease(
        std::shared_ptr<void> object,
        const FrameContextSnapshot& frame_context)
    {
        if (!object)
        {
            return;
        }

        const unsigned delay_frame = ResolveDeferredReleaseLatencyFrames(frame_context);
        m_deferred_release_queue.EnqueueRetainedObject(std::move(object), m_frame_index, delay_frame);
    }

    void RenderGraph::EnqueueBufferDescriptorEntryForDeferredRelease(
        const RenderPassDescriptorResource::BufferDescriptorCacheEntry& cache_entry,
        const FrameContextSnapshot& frame_context)
    {
        if (cache_entry.descriptor)
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(cache_entry.descriptor), frame_context);
        }
    }

    void RenderGraph::EnqueueTextureDescriptorEntryForDeferredRelease(
        const RenderPassDescriptorResource::TextureDescriptorCacheEntry& cache_entry,
        const FrameContextSnapshot& frame_context)
    {
        if (cache_entry.descriptor)
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(cache_entry.descriptor), frame_context);
        }
        if (cache_entry.descriptor_table)
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(cache_entry.descriptor_table), frame_context);
        }
        for (const auto& source_descriptor : cache_entry.descriptor_table_source_data)
        {
            if (source_descriptor)
            {
                EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(source_descriptor), frame_context);
            }
        }
    }

    void RenderGraph::EnqueueBufferDescriptorForDeferredRelease(
        RenderPassDescriptorResource& descriptor_resource,
        const std::string& binding_name,
        const FrameContextSnapshot& frame_context)
    {
        const auto binding_cache_it = descriptor_resource.m_buffer_descriptor_cache.find(binding_name);
        if (binding_cache_it == descriptor_resource.m_buffer_descriptor_cache.end())
        {
            return;
        }

        for (const auto& cache_pair : binding_cache_it->second)
        {
            EnqueueBufferDescriptorEntryForDeferredRelease(cache_pair.second, frame_context);
        }
        descriptor_resource.m_buffer_descriptor_cache.erase(binding_cache_it);
    }

    void RenderGraph::EnqueueTextureDescriptorForDeferredRelease(
        RenderPassDescriptorResource& descriptor_resource,
        const std::string& binding_name,
        const FrameContextSnapshot& frame_context)
    {
        const auto release_texture_cache_pool =
            [this, &frame_context](std::map<unsigned long long, RenderPassDescriptorResource::TextureDescriptorCacheEntry>& cache_pool)
        {
            for (const auto& cache_pair : cache_pool)
            {
                EnqueueTextureDescriptorEntryForDeferredRelease(cache_pair.second, frame_context);
            }
        };

        const auto texture_cache_it = descriptor_resource.m_texture_descriptor_cache.find(binding_name);
        if (texture_cache_it != descriptor_resource.m_texture_descriptor_cache.end())
        {
            release_texture_cache_pool(texture_cache_it->second);
            descriptor_resource.m_texture_descriptor_cache.erase(texture_cache_it);
        }

        const auto render_target_texture_cache_it = descriptor_resource.m_render_target_texture_descriptor_cache.find(binding_name);
        if (render_target_texture_cache_it != descriptor_resource.m_render_target_texture_descriptor_cache.end())
        {
            release_texture_cache_pool(render_target_texture_cache_it->second);
            descriptor_resource.m_render_target_texture_descriptor_cache.erase(render_target_texture_cache_it);
        }
    }

    void RenderGraph::ReleaseRenderPassDescriptorResource(
        RenderPassDescriptorResource& descriptor_resource,
        const FrameContextSnapshot& frame_context)
    {
        for (const auto& binding_cache_pair : descriptor_resource.m_buffer_descriptor_cache)
        {
            for (const auto& cache_pair : binding_cache_pair.second)
            {
                EnqueueBufferDescriptorEntryForDeferredRelease(cache_pair.second, frame_context);
            }
        }

        const auto release_texture_cache =
            [this, &frame_context](const std::map<std::string, std::map<unsigned long long, RenderPassDescriptorResource::TextureDescriptorCacheEntry>>& texture_cache_map)
        {
            for (const auto& binding_cache_pair : texture_cache_map)
            {
                for (const auto& cache_pair : binding_cache_pair.second)
                {
                    EnqueueTextureDescriptorEntryForDeferredRelease(cache_pair.second, frame_context);
                }
            }
        };

        release_texture_cache(descriptor_resource.m_texture_descriptor_cache);
        release_texture_cache(descriptor_resource.m_render_target_texture_descriptor_cache);

        descriptor_resource.m_buffer_descriptor_cache.clear();
        descriptor_resource.m_texture_descriptor_cache.clear();
        descriptor_resource.m_render_target_texture_descriptor_cache.clear();
    }

    void RenderGraph::PruneDescriptorResources(
        RenderPassDescriptorResource& descriptor_resource,
        const RenderPassDrawDesc& draw_info,
        const FrameContextSnapshot& frame_context)
    {
        const unsigned descriptor_cache_retention_frame = ResolveDescriptorCacheRetentionFrames(frame_context);
        const auto should_release_stale_entry = [this, descriptor_cache_retention_frame](unsigned long long last_used_frame)
        {
            return m_frame_index >= last_used_frame + descriptor_cache_retention_frame;
        };

        for (auto it = descriptor_resource.m_buffer_descriptor_cache.begin(); it != descriptor_resource.m_buffer_descriptor_cache.end(); )
        {
            if (!draw_info.buffer_resources.contains(it->first))
            {
                for (const auto& cache_pair : it->second)
                {
                    EnqueueBufferDescriptorEntryForDeferredRelease(cache_pair.second, frame_context);
                }
                it = descriptor_resource.m_buffer_descriptor_cache.erase(it);
            }
            else
            {
                auto& binding_cache = it->second;
                for (auto cache_it = binding_cache.begin(); cache_it != binding_cache.end(); )
                {
                    if (should_release_stale_entry(cache_it->second.last_used_frame))
                    {
                        EnqueueBufferDescriptorEntryForDeferredRelease(cache_it->second, frame_context);
                        cache_it = binding_cache.erase(cache_it);
                    }
                    else
                    {
                        ++cache_it;
                    }
                }

                if (binding_cache.empty())
                {
                    it = descriptor_resource.m_buffer_descriptor_cache.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        const auto should_keep_texture_binding = [&draw_info](const std::string& binding_name)
        {
            return draw_info.texture_resources.contains(binding_name) || draw_info.render_target_texture_resources.contains(binding_name);
        };

        const auto prune_texture_cache =
            [this, &frame_context, &should_keep_texture_binding, &should_release_stale_entry](std::map<std::string, std::map<unsigned long long, RenderPassDescriptorResource::TextureDescriptorCacheEntry>>& texture_cache_map)
        {
            for (auto binding_it = texture_cache_map.begin(); binding_it != texture_cache_map.end(); )
            {
                if (!should_keep_texture_binding(binding_it->first))
                {
                    for (const auto& cache_pair : binding_it->second)
                    {
                        EnqueueTextureDescriptorEntryForDeferredRelease(cache_pair.second, frame_context);
                    }
                    binding_it = texture_cache_map.erase(binding_it);
                }
                else
                {
                    auto& binding_cache = binding_it->second;
                    for (auto cache_it = binding_cache.begin(); cache_it != binding_cache.end(); )
                    {
                        if (should_release_stale_entry(cache_it->second.last_used_frame))
                        {
                            EnqueueTextureDescriptorEntryForDeferredRelease(cache_it->second, frame_context);
                            cache_it = binding_cache.erase(cache_it);
                        }
                        else
                        {
                            ++cache_it;
                        }
                    }

                    if (binding_cache.empty())
                    {
                        binding_it = texture_cache_map.erase(binding_it);
                    }
                    else
                    {
                        ++binding_it;
                    }
                }
            }
        };

        prune_texture_cache(descriptor_resource.m_texture_descriptor_cache);
        prune_texture_cache(descriptor_resource.m_render_target_texture_descriptor_cache);
    }

    void RenderGraph::CollectUnusedRenderPassDescriptorResources(const FrameContextSnapshot& frame_context)
    {
        const unsigned retention_frame = ResolveRenderPassDescriptorRetentionFrames(frame_context);
        const auto expired_handles = m_descriptor_resource_store.CollectExpiredInactiveHandles(
            m_render_graph_node_handles,
            m_frame_index,
            retention_frame);
        for (const auto node_handle : expired_handles)
        {
            auto* descriptor_resource = m_descriptor_resource_store.Find(node_handle);
            if (!descriptor_resource)
            {
                continue;
            }

            ReleaseRenderPassDescriptorResource(*descriptor_resource, frame_context);
            m_descriptor_resource_store.Erase(node_handle);
        }
    }

    void RenderGraph::FlushDeferredResourceReleases(bool force_release_all)
    {
        auto& memory_manager = m_resource_allocator.GetMemoryManager();
        m_deferred_release_queue.Flush(memory_manager, m_frame_index, force_release_all);
    }

    void RenderGraph::ApplyPendingRenderStateUpdates(const FrameContextSnapshot& frame_context)
    {
        for (auto it = m_pending_render_state_updates.begin(); it != m_pending_render_state_updates.end(); )
        {
            const RenderGraphNodeHandle node_handle = it->first;
            if (!m_render_graph_node_handles.contains(node_handle) ||
                node_handle.value >= m_render_graph_nodes.size())
            {
                it = m_pending_render_state_updates.erase(it);
                continue;
            }

            auto& node_desc = m_render_graph_nodes[node_handle.value];
            auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(node_desc.render_pass_handle);
            if (!render_pass)
            {
                LOG_FORMAT_FLUSH("[RenderGraph][StateUpdate][Warn] Node %u has no render pass. Keep queued render-state update.\n",
                                 node_handle.value);
                ++it;
                continue;
            }

            std::shared_ptr<IRHIResource> retired_pipeline_state_object;
            if (!render_pass->UpdateGraphicsRenderState(
                    m_resource_allocator.GetDevice(),
                    m_resource_allocator.GetCurrentSwapchain(),
                    it->second.render_state,
                    retired_pipeline_state_object))
            {
                LOG_FORMAT_FLUSH("[RenderGraph][StateUpdate][Warn] Failed to apply queued render-state update for node %u.\n",
                                 node_handle.value);
                ++it;
                continue;
            }

            if (retired_pipeline_state_object)
            {
                EnqueueResourceForDeferredRelease(retired_pipeline_state_object, frame_context);
            }

            node_desc.render_state = it->second.render_state;
            it = m_pending_render_state_updates.erase(it);
        }
    }

    unsigned RenderGraph::GetCrossFrameComparisonWindowSize(const FrameContextSnapshot& frame_context) const
    {
        return ResolveCrossFrameComparisonWindowSize(frame_context);
    }

    unsigned RenderGraph::GetCrossFrameHazardSlotIndex(
        const FrameContextSnapshot& frame_context,
        unsigned window_size) const
    {
        if (!frame_context.per_frame_resource_binding_enabled || window_size == 0u)
        {
            return 0u;
        }

        const unsigned hazard_slot_index = frame_context.frame_slot_index % window_size;
        GLTF_CHECK(hazard_slot_index < window_size);
        return hazard_slot_index;
    }

    void RenderGraph::LogRenderPassValidationResult(RenderGraphNodeHandle render_graph_node_handle,
                                                    const RenderGraphNodeDesc& render_graph_node_desc,
                                                    bool valid,
                                                    const std::vector<std::string>& errors,
                                                    const std::vector<std::string>& warnings)
    {
        RenderPass::DrawValidationResult validation_result{};
        validation_result.valid = valid;
        validation_result.errors = errors;
        validation_result.warnings = warnings;
        if (!RenderGraphExecutionPolicy::ShouldEmitValidationLog(
            m_render_pass_validation_last_log_frame,
            m_render_pass_validation_last_message_hash,
            render_graph_node_handle,
            validation_result,
            m_frame_index,
            m_validation_policy.log_interval_frames))
        {
            return;
        }

        const char* group_name = render_graph_node_desc.debug_group.empty() ? "<group-empty>" : render_graph_node_desc.debug_group.c_str();
        const char* pass_name = render_graph_node_desc.debug_name.empty() ? "<pass-empty>" : render_graph_node_desc.debug_name.c_str();

        if (!errors.empty())
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Validation] Node %u (%s/%s) has %zu error(s) and %zu warning(s). Skip execution.\n",
                             render_graph_node_handle.value,
                             group_name,
                             pass_name,
                             errors.size(),
                             warnings.size());
        }
        else if (!warnings.empty())
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Validation] Node %u (%s/%s) has %zu warning(s)%s.\n",
                             render_graph_node_handle.value,
                             group_name,
                             pass_name,
                             warnings.size(),
                             m_validation_policy.skip_execution_on_warning ? " and will be skipped by policy" : "");
        }

        for (const auto& error : errors)
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Validation][Error]   %s\n", error.c_str());
        }
        for (const auto& warning : warnings)
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Validation][Warn ]   %s\n", warning.c_str());
        }
    }

    RenderGraph::RenderPassExecutionStatus RenderGraph::ExecuteRenderGraphNode(
        IRHICommandList& command_list,
        const FrameContextSnapshot& frame_context,
        RenderGraphNodeHandle render_graph_node_handle,
        unsigned long long interval)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        auto& render_graph_node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        if (render_graph_node_desc.pre_render_callback)
        {
            render_graph_node_desc.pre_render_callback(interval);
        }
        
        auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(render_graph_node_desc.render_pass_handle);
        if (!render_pass)
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Validation] Node %u has no render pass resource. Skip execution.\n",
                render_graph_node_handle.value);
            return RenderPassExecutionStatus::SKIPPED_MISSING_RENDER_PASS;
        }
        const auto validation_result = render_pass->ValidateDrawDesc(render_graph_node_desc.draw_info);
        if (!validation_result.errors.empty() || !validation_result.warnings.empty())
        {
            LogRenderPassValidationResult(
                render_graph_node_handle,
                render_graph_node_desc,
                validation_result.valid,
                validation_result.errors,
                validation_result.warnings);
        }
        if (RenderGraphExecutionPolicy::ShouldSkipExecution(validation_result, m_validation_policy.skip_execution_on_warning))
        {
            return RenderPassExecutionStatus::SKIPPED_INVALID_DRAW_DESC;
        }

        if (!RHIUtilInstanceManager::Instance().SetPipelineState(command_list, render_pass->GetPipelineStateObject()))
        {
            const char* group_name = render_graph_node_desc.debug_group.empty() ? "<group-empty>" : render_graph_node_desc.debug_group.c_str();
            const char* pass_name = render_graph_node_desc.debug_name.empty() ? "<pass-empty>" : render_graph_node_desc.debug_name.c_str();
            LOG_FORMAT_FLUSH("[RenderGraph][Validation] Node %u (%s/%s) failed to bind pipeline state. Skip execution.\n",
                             render_graph_node_handle.value,
                             group_name,
                             pass_name);
            return RenderPassExecutionStatus::SKIPPED_INVALID_DRAW_DESC;
        }
        RHIUtilInstanceManager::Instance().SetRootSignature(command_list, render_pass->GetRootSignature(), render_pass->GetPipelineStateObject(), RendererInterfaceRHIConverter::ConvertToRHIPipelineType(render_pass->GetRenderPassType()));
        RHIUtilInstanceManager::Instance().SetPrimitiveTopology(command_list, ConvertToRHIPrimitiveTopology(render_pass->GetPrimitiveTopology()));

        unsigned default_viewport_width = m_window.GetWidth();
        unsigned default_viewport_height = m_window.GetHeight();
        if (!render_graph_node_desc.draw_info.render_target_resources.empty())
        {
            const auto first_render_target_handle = render_graph_node_desc.draw_info.render_target_resources.begin()->first;
            auto first_render_target = InternalResourceHandleTable::Instance().GetRenderTarget(first_render_target_handle);
            default_viewport_width = first_render_target->m_source->GetTextureDesc().GetTextureWidth();
            default_viewport_height = first_render_target->m_source->GetTextureDesc().GetTextureHeight();

            for (const auto& render_target_info : render_graph_node_desc.draw_info.render_target_resources)
            {
                auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_info.first);
                default_viewport_width = (std::min)(default_viewport_width, render_target->m_source->GetTextureDesc().GetTextureWidth());
                default_viewport_height = (std::min)(default_viewport_height, render_target->m_source->GetTextureDesc().GetTextureHeight());
            }
        }

        RHIViewportDesc viewport{};
        viewport.width = render_pass->GetViewportSize().first >= 0 ? render_pass->GetViewportSize().first : default_viewport_width;
        viewport.height = render_pass->GetViewportSize().second >= 0 ? render_pass->GetViewportSize().second : default_viewport_height;
        viewport.min_depth = 0.f;
        viewport.max_depth = 1.f;
        viewport.top_left_x = 0.f;
        viewport.top_left_y = 0.f;
        RHIUtilInstanceManager::Instance().SetViewport(command_list, viewport);

        const RHIScissorRectDesc scissor_rect =
            {
            (unsigned)viewport.top_left_x,
            (unsigned)viewport.top_left_y,
            (unsigned)(viewport.top_left_x + viewport.width),
            (unsigned)(viewport.top_left_y + viewport.height)
        }; 
        RHIUtilInstanceManager::Instance().SetScissorRect(command_list, scissor_rect);

        RHIBeginRenderingInfo begin_rendering_info{};
        
        // render target binding
        bool clear_render_target = false;
        bool clear_depth_stencil = false;
        for (const auto& render_target_info : render_graph_node_desc.draw_info.render_target_resources)
        {
            GLTF_CHECK(render_target_info.first != NULL_HANDLE);
            auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_info.first);
            begin_rendering_info.m_render_targets.push_back(render_target.get());

            RenderPassAttachmentLoadOp load_op = render_target_info.second.load_op;
            if (render_target_info.second.need_clear && load_op == RenderPassAttachmentLoadOp::LOAD)
            {
                load_op = RenderPassAttachmentLoadOp::CLEAR;
            }

            RHIAttachmentLoadOp rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_LOAD;
            switch (load_op)
            {
            case RenderPassAttachmentLoadOp::LOAD:
                rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_LOAD;
                break;
            case RenderPassAttachmentLoadOp::CLEAR:
                rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_CLEAR;
                break;
            case RenderPassAttachmentLoadOp::DONT_CARE:
                rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_DONT_CARE;
                break;
            }

            RHIAttachmentStoreOp rhi_store_op = RHIAttachmentStoreOp::STORE_OP_STORE;
            switch (render_target_info.second.store_op)
            {
            case RenderPassAttachmentStoreOp::STORE:
                rhi_store_op = RHIAttachmentStoreOp::STORE_OP_STORE;
                break;
            case RenderPassAttachmentStoreOp::DONT_CARE:
                rhi_store_op = RHIAttachmentStoreOp::STORE_OP_DONT_CARE;
                break;
            }

            begin_rendering_info.m_render_target_load_ops.push_back(rhi_load_op);
            begin_rendering_info.m_render_target_store_ops.push_back(rhi_store_op);

            if (render_target_info.second.usage == RenderPassResourceUsage::DEPTH_STENCIL)
            {
                begin_rendering_info.enable_depth_write = true;
                if (rhi_load_op == RHIAttachmentLoadOp::LOAD_OP_CLEAR)
                {
                    clear_depth_stencil = true;
                }
            }
            else if (rhi_load_op == RHIAttachmentLoadOp::LOAD_OP_CLEAR)
            {
                clear_render_target = true;
            }
        }

        begin_rendering_info.rendering_area_offset_x = viewport.top_left_x;
        begin_rendering_info.rendering_area_offset_y = viewport.top_left_y;
        begin_rendering_info.rendering_area_width = viewport.width;
        begin_rendering_info.rendering_area_height = viewport.height;
        begin_rendering_info.clear_render_target = clear_render_target;
        begin_rendering_info.clear_depth_stencil = clear_depth_stencil;

        // Bind descriptor heap
        m_resource_allocator.GetDescriptorManager().BindDescriptorContext(command_list);

        // buffer binding
        RHIPipelineType pipeline_type = RHIPipelineType::Unknown;
        switch (render_pass->GetRenderPassType()) {
        case RenderPassType::GRAPHICS:
            pipeline_type = RHIPipelineType::Graphics;
            break;
        case RenderPassType::COMPUTE:
            pipeline_type = RHIPipelineType::Compute;
            break;
        case RenderPassType::RAY_TRACING:
            pipeline_type = RHIPipelineType::RayTracing;
            break;
        }

        auto& render_pass_descriptor_resource = m_descriptor_resource_store.GetOrCreate(render_graph_node_handle);
        m_descriptor_resource_store.MarkUsed(render_graph_node_handle, m_frame_index);
        PruneDescriptorResources(render_pass_descriptor_resource, render_graph_node_desc.draw_info, frame_context);
        
        for (const auto& buffer : render_graph_node_desc.draw_info.buffer_resources)
        {
            if (!render_pass->HasRootSignatureAllocation(buffer.first))
            {
                continue;
            }
            GLTF_CHECK(buffer.second.buffer_handle != NULL_HANDLE);
            const auto& root_signature_allocations = render_pass->GetRootSignatureAllocations(buffer.first);
            auto buffer_handle = buffer.second.buffer_handle;
            auto buffer_allocation = RendererInterface::InternalResourceHandleTable::Instance().GetBuffer(buffer_handle);
            GLTF_CHECK(buffer_allocation && buffer_allocation->m_buffer);
            auto buffer_size = buffer_allocation->m_buffer->GetBufferDesc().width;
            const auto buffer_identity_key = reinterpret_cast<std::uintptr_t>(buffer_allocation->m_buffer.get());
            const unsigned long long buffer_descriptor_cache_key = ComputeBufferDescriptorCacheKey(buffer.second, buffer_identity_key);

            auto& binding_cache = render_pass_descriptor_resource.m_buffer_descriptor_cache[buffer.first];
            auto buffer_cache_it = binding_cache.find(buffer_descriptor_cache_key);
            if (buffer_cache_it == binding_cache.end())
            {
                std::shared_ptr<IRHIBufferDescriptorAllocation> buffer_descriptor = nullptr;
                switch (buffer.second.binding_type)
                {
                case BufferBindingDesc::CBV:
                    {
                        RHIBufferDescriptorDesc buffer_descriptor_desc(RHIDataFormat::UNKNOWN, RHIViewType::RVT_CBV, buffer_size, 0);
                        m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), buffer_allocation->m_buffer, buffer_descriptor_desc, buffer_descriptor);
                    }
                    break;
                case BufferBindingDesc::SRV:
                    {
                        RHISRVStructuredBufferDesc srv_buffer_desc{buffer.second.stride, buffer.second.count, buffer.second.is_structured_buffer};
                        RHIBufferDescriptorDesc buffer_descriptor_desc(RHIDataFormat::UNKNOWN, RHIViewType::RVT_SRV, buffer_size, 0, srv_buffer_desc);
                        m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), buffer_allocation->m_buffer, buffer_descriptor_desc, buffer_descriptor);
                    }
                    break;
                case BufferBindingDesc::UAV:
                    {
                        RHIUAVStructuredBufferDesc uav_buffer_desc{buffer.second.stride, buffer.second.count, buffer.second.is_structured_buffer, buffer.second.use_count_buffer, buffer.second.count_buffer_offset};
                        RHIBufferDescriptorDesc buffer_descriptor_desc(RHIDataFormat::UNKNOWN, RHIViewType::RVT_UAV, buffer_size, 0, uav_buffer_desc);
                        m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), buffer_allocation->m_buffer, buffer_descriptor_desc, buffer_descriptor);
                    }
                    break;
                }
                GLTF_CHECK(buffer_descriptor);

                RenderPassDescriptorResource::BufferDescriptorCacheEntry cache_entry{};
                cache_entry.binding_desc = buffer.second;
                cache_entry.buffer_identity_key = buffer_identity_key;
                cache_entry.descriptor = buffer_descriptor;
                cache_entry.last_used_frame = m_frame_index;
                binding_cache[buffer_descriptor_cache_key] = std::move(cache_entry);
                buffer_cache_it = binding_cache.find(buffer_descriptor_cache_key);
            }
            GLTF_CHECK(buffer_cache_it != binding_cache.end());
            auto& buffer_cache_entry = buffer_cache_it->second;
            buffer_cache_entry.last_used_frame = m_frame_index;

            auto buffer_descriptor = buffer_cache_entry.descriptor;
            switch (buffer.second.binding_type) {
            case BufferBindingDesc::CBV:
                buffer_allocation->m_buffer->Transition(command_list, RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER);
                break;
            case BufferBindingDesc::SRV:
                //buffer_allocation->m_buffer->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);
                //break;
            case BufferBindingDesc::UAV:
                buffer_allocation->m_buffer->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);
                break;
            }
            for (const auto& root_signature_allocation : root_signature_allocations)
            {
                render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *buffer_descriptor);
            }
        }

        for (const auto& texture  :render_graph_node_desc.draw_info.texture_resources)
        {
            if (!render_pass->HasRootSignatureAllocation(texture.first))
            {
                continue;
            }
            GLTF_CHECK(!texture.second.textures.empty());
            const bool is_texture_table = texture.second.textures.size() > 1;
            const auto& root_signature_allocations = render_pass->GetRootSignatureAllocations(texture.first);

            const auto source_texture_identity_keys = BuildTextureSourceIdentityKeys(texture.second);
            const unsigned long long texture_descriptor_cache_key =
                ComputeTextureDescriptorCacheKey(static_cast<unsigned>(texture.second.type), source_texture_identity_keys);
            auto& binding_cache = render_pass_descriptor_resource.m_texture_descriptor_cache[texture.first];
            auto texture_cache_it = binding_cache.find(texture_descriptor_cache_key);
            if (texture_cache_it == binding_cache.end())
            {
                std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> descriptor_allocations;
                for (const auto handle : texture.second.textures)
                {
                    auto texture_allocation = InternalResourceHandleTable::Instance().GetTexture(handle);
                    GLTF_CHECK(texture_allocation && texture_allocation->m_texture);
                    RHITextureDescriptorDesc texture_descriptor_desc{texture_allocation->m_texture->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, texture.second.type == TextureBindingDesc::SRV? RHIViewType::RVT_SRV :RHIViewType::RVT_UAV};
                    std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor = nullptr;
                    m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), texture_allocation->m_texture, texture_descriptor_desc, texture_descriptor);
                    descriptor_allocations.push_back(texture_descriptor);
                }

                RenderPassDescriptorResource::TextureDescriptorCacheEntry cache_entry{};
                cache_entry.binding_type = static_cast<unsigned>(texture.second.type);
                cache_entry.source_texture_identity_keys = source_texture_identity_keys;
                if (is_texture_table)
                {
                    std::shared_ptr<IRHIDescriptorTable> descriptor_table = RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
                    const bool built = descriptor_table->Build(m_resource_allocator.GetDevice(), descriptor_allocations);
                    GLTF_CHECK(built);
                    cache_entry.descriptor_table = descriptor_table;
                    cache_entry.descriptor_table_source_data = descriptor_allocations;
                }
                else
                {
                    GLTF_CHECK(descriptor_allocations.size() == 1);
                    cache_entry.descriptor = descriptor_allocations.at(0);
                }

                cache_entry.last_used_frame = m_frame_index;
                binding_cache[texture_descriptor_cache_key] = std::move(cache_entry);
                texture_cache_it = binding_cache.find(texture_descriptor_cache_key);
            }
            GLTF_CHECK(texture_cache_it != binding_cache.end());
            auto& texture_cache_entry = texture_cache_it->second;
            texture_cache_entry.last_used_frame = m_frame_index;

            if (is_texture_table)
            {
                auto descriptor_table = texture_cache_entry.descriptor_table;
                auto& table_texture_descriptors = texture_cache_entry.descriptor_table_source_data;
                GLTF_CHECK(descriptor_table);
                for (const auto& table_texture : table_texture_descriptors)
                {
                    table_texture->m_source->Transition(command_list, texture.second.type == TextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);
                }

                for (const auto& root_signature_allocation : root_signature_allocations)
                {
                    render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor_table, texture.second.type == TextureBindingDesc::SRV? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV);
                }
            }
            else
            {
                auto descriptor = texture_cache_entry.descriptor;
                GLTF_CHECK(descriptor);
                descriptor->m_source->Transition(command_list, texture.second.type == TextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);

                for (const auto& root_signature_allocation : root_signature_allocations)
                {
                    render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor);
                }
            }
        }

        const auto get_render_target_texture_state =
            [](const std::shared_ptr<IRHITextureDescriptorAllocation>& descriptor_allocation,
                RenderTargetTextureBindingDesc::TextureBindingType binding_type)
        {
            if (binding_type != RenderTargetTextureBindingDesc::SRV)
            {
                return RHIResourceStateType::STATE_UNORDERED_ACCESS;
            }

            const bool is_depth_texture =
                descriptor_allocation &&
                descriptor_allocation->m_source &&
                ((descriptor_allocation->m_source->GetTextureDesc().GetUsage() & RUF_ALLOW_DEPTH_STENCIL) != 0);
            return is_depth_texture
                ? RHIResourceStateType::STATE_DEPTH_READ
                : RHIResourceStateType::STATE_ALL_SHADER_RESOURCE;
        };

        for (const auto& render_target_pair  :render_graph_node_desc.draw_info.render_target_texture_resources)
        {
            if (!render_pass->HasRootSignatureAllocation(render_target_pair.first))
            {
                continue;
            }
            GLTF_CHECK(!render_target_pair.second.render_target_texture.empty());
            const bool is_texture_table = render_target_pair.second.render_target_texture.size() > 1;

            const auto source_texture_identity_keys = BuildRenderTargetTextureSourceIdentityKeys(render_target_pair.second);
            const unsigned long long texture_descriptor_cache_key =
                ComputeTextureDescriptorCacheKey(static_cast<unsigned>(render_target_pair.second.type), source_texture_identity_keys);
            auto& binding_cache = render_pass_descriptor_resource.m_render_target_texture_descriptor_cache[render_target_pair.first];
            auto texture_cache_it = binding_cache.find(texture_descriptor_cache_key);
            if (texture_cache_it == binding_cache.end())
            {
                std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> descriptor_allocations;

                for (const auto& render_target : render_target_pair.second.render_target_texture)
                {
                    auto render_target_allocation = InternalResourceHandleTable::Instance().GetRenderTarget(render_target);
                    GLTF_CHECK(render_target_allocation && render_target_allocation->m_source);
                    auto texture = render_target_allocation->m_source;
                    const bool is_depth_render_target =
                        (texture->GetTextureDesc().GetUsage() & RUF_ALLOW_DEPTH_STENCIL) != 0;
                    RHITextureDescriptorDesc texture_descriptor_desc{
                        texture->GetTextureFormat(),
                        RHIResourceDimension::TEXTURE2D,
                        render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV ? RHIViewType::RVT_SRV : RHIViewType::RVT_UAV};
                    if (is_depth_render_target && render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV)
                    {
                        texture_descriptor_desc.m_format = RHIDataFormat::D32_SAMPLE_RESERVED;
                    }
                    std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor = nullptr;
                    m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), texture, texture_descriptor_desc, texture_descriptor);
                    descriptor_allocations.push_back(texture_descriptor);
                }
                
                RenderPassDescriptorResource::TextureDescriptorCacheEntry cache_entry{};
                cache_entry.binding_type = static_cast<unsigned>(render_target_pair.second.type);
                cache_entry.source_texture_identity_keys = source_texture_identity_keys;
                if (is_texture_table)
                {
                    std::shared_ptr<IRHIDescriptorTable> descriptor_table = RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
                    const bool built = descriptor_table->Build(m_resource_allocator.GetDevice(), descriptor_allocations);
                    GLTF_CHECK(built);
                    cache_entry.descriptor_table = descriptor_table;
                    cache_entry.descriptor_table_source_data = descriptor_allocations;
                }
                else
                {
                    GLTF_CHECK(render_target_pair.second.render_target_texture.size() == 1);
                    cache_entry.descriptor = descriptor_allocations[0];
                }

                cache_entry.last_used_frame = m_frame_index;
                binding_cache[texture_descriptor_cache_key] = std::move(cache_entry);
                texture_cache_it = binding_cache.find(texture_descriptor_cache_key);
            }
            GLTF_CHECK(texture_cache_it != binding_cache.end());
            auto& texture_cache_entry = texture_cache_it->second;
            texture_cache_entry.last_used_frame = m_frame_index;

            const auto& root_signature_allocations = render_pass->GetRootSignatureAllocations(render_target_pair.first);
            if (is_texture_table)
            {
                auto descriptor_table = texture_cache_entry.descriptor_table;
                auto& table_texture_descriptors = texture_cache_entry.descriptor_table_source_data;
                GLTF_CHECK(descriptor_table);
                for (const auto& table_texture : table_texture_descriptors)
                {
                    table_texture->m_source->Transition(
                        command_list,
                        get_render_target_texture_state(table_texture, render_target_pair.second.type));
                }

                for (const auto& root_signature_allocation : root_signature_allocations)
                {
                    render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor_table, render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV);
                }
            }
            else
            {
                auto descriptor = texture_cache_entry.descriptor;
                GLTF_CHECK(descriptor);
                descriptor->m_source->Transition(
                    command_list,
                    get_render_target_texture_state(descriptor, render_target_pair.second.type));
                for (const auto& root_signature_allocation : root_signature_allocations)
                {
                    render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor);
                }
            }
        }

        render_pass->GetDescriptorUpdater().FinalizeUpdateDescriptors(m_resource_allocator.GetDevice(), command_list, render_pass->GetRootSignature());

        if (pipeline_type == RHIPipelineType::Graphics)
        {
            RHIUtilInstanceManager::Instance().BeginRendering(command_list, begin_rendering_info);    
        }
        
        const auto& draw_info = render_graph_node_desc.draw_info;
        for (const auto& command : draw_info.execute_commands)
        {
            switch (command.type) {
            case ExecuteCommandType::DRAW_VERTEX_COMMAND:
                RHIUtilInstanceManager::Instance().DrawInstanced(command_list,
                    command.parameter.draw_vertex_command_parameter.vertex_count,
                    1,
                    command.parameter.draw_vertex_command_parameter.start_vertex_location,
                    0);
                break;
            case ExecuteCommandType::DRAW_VERTEX_INSTANCING_COMMAND:
                RHIUtilInstanceManager::Instance().DrawInstanced(command_list,
                    command.parameter.draw_vertex_instance_command_parameter.vertex_count,
                    command.parameter.draw_vertex_instance_command_parameter.instance_count,
                    command.parameter.draw_vertex_instance_command_parameter.start_vertex_location,
                    command.parameter.draw_vertex_instance_command_parameter.start_instance_location);
                break;
            case ExecuteCommandType::DRAW_INDEXED_COMMAND:
                break;
            case ExecuteCommandType::DRAW_INDEXED_INSTANCING_COMMAND:
                {
                    auto indexed_buffer_view = InternalResourceHandleTable::Instance().GetIndexBufferView(command.input_buffer.index_buffer_handle);
                    //auto indexed_buffer = InternalResourceHandleTable::Instance().GetIndexBuffer(command.input_buffer.index_buffer_handle);
                    //indexed_buffer->GetBuffer().Transition(command_list, RHIResourceStateType::STATE_INDEX_BUFFER);
                    RHIUtilInstanceManager::Instance().SetIndexBufferView(command_list, *indexed_buffer_view);
                    RHIUtilInstanceManager::Instance().DrawIndexInstanced(command_list,
                        command.parameter.draw_indexed_instance_command_parameter.index_count_per_instance,
                        command.parameter.draw_indexed_instance_command_parameter.instance_count,
                        command.parameter.draw_indexed_instance_command_parameter.start_index_location,
                        command.parameter.draw_indexed_instance_command_parameter.start_vertex_location,
                        command.parameter.draw_indexed_instance_command_parameter.start_instance_location);
                    break;    
                }
            case ExecuteCommandType::COMPUTE_DISPATCH_COMMAND:
                RHIUtilInstanceManager::Instance().Dispatch(command_list, command.parameter.dispatch_parameter.group_size_x, command.parameter.dispatch_parameter.group_size_y, command.parameter.dispatch_parameter.group_size_z);
                break;
            case ExecuteCommandType::RAY_TRACING_COMMAND:
                break;
            }    
        }
        
        if (pipeline_type == RHIPipelineType::Graphics)
        {
            RHIUtilInstanceManager::Instance().EndRendering(command_list);
        }
        return RenderPassExecutionStatus::EXECUTED;
    }

    void RenderGraph::CloseCurrentCommandListAndExecute(IRHICommandList& command_list, const RHIExecuteCommandListContext& context, bool wait)
    {
        const bool closed = RHIUtilInstanceManager::Instance().CloseCommandList(command_list);
        GLTF_CHECK(closed);
        
        auto& command_queue = m_resource_allocator.GetCommandQueue();
        
        GLTF_CHECK(RHIUtilInstanceManager::Instance().ExecuteCommandList(command_list, command_queue, context));
        if (wait)
        {
            RHIUtilInstanceManager::Instance().WaitCommandListFinish(command_list);
        }
    }

    void RenderGraph::Present(IRHICommandList& command_list, const FrameContextSnapshot& frame_context)
    {
        m_resource_allocator.GetCurrentSwapchainRT(frame_context).m_source->Transition(command_list, RHIResourceStateType::STATE_PRESENT);

        RHIExecuteCommandListContext context;
        context.wait_infos.push_back({&m_resource_allocator.GetCurrentSwapchain().GetAvailableFrameSemaphore(), RHIPipelineStage::COLOR_ATTACHMENT_OUTPUT});
        context.sign_semaphores.push_back(&command_list.GetSemaphore());
        const auto submit_begin = std::chrono::steady_clock::now();
        CloseCurrentCommandListAndExecute(command_list, context, false);
        const auto submit_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.submit_command_list_ms = ToMilliseconds(submit_begin, submit_end);
    
        const auto present_call_begin = std::chrono::steady_clock::now();
        const bool present_succeeded = RHIUtilInstanceManager::Instance().Present(
            m_resource_allocator.GetCurrentSwapchain(),
            m_resource_allocator.GetCommandQueue(),
            command_list);
        const auto present_call_end = std::chrono::steady_clock::now();
        m_current_frame_timing_breakdown.present_call_ms = ToMilliseconds(present_call_begin, present_call_end);
        if (!present_succeeded)
        {
            m_resource_allocator.NotifySwapchainPresentFailure();
            return;
        }
    }
}

