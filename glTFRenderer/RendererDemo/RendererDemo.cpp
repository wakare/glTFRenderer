#include <memory>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cctype>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <crtdbg.h>
#endif

#include "DemoApps/DemoAppModelViewer.h"
#include "DemoApps/DemoAppModelViewerFrostedGlass.h"
#include "DemoApps/DemoTriangleApp.h"

#define REGISTER_DEMO_APP(demo_name, app_name) if (demo_name == #app_name) {demo = std::make_unique<app_name>();} 

namespace
{
    bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs.size(); ++i)
        {
            const unsigned char l = static_cast<unsigned char>(lhs[i]);
            const unsigned char r = static_cast<unsigned char>(rhs[i]);
            if (std::tolower(l) != std::tolower(r))
            {
                return false;
            }
        }
        return true;
    }

    bool IsTruthyEnvVar(const char* name)
    {
#ifdef _WIN32
        char* value = nullptr;
        size_t value_len = 0;
        const errno_t env_result = _dupenv_s(&value, &value_len, name);
        if (env_result != 0 || !value || value_len == 0)
        {
            if (value)
            {
                free(value);
            }
            return false;
        }

        const std::string_view view(value);
        const bool enabled = EqualsIgnoreCase(view, "1") ||
                             EqualsIgnoreCase(view, "true") ||
                             EqualsIgnoreCase(view, "yes") ||
                             EqualsIgnoreCase(view, "on");
        free(value);
        return enabled;
#else
        const char* value = std::getenv(name);
        if (!value)
        {
            return false;
        }

        const std::string_view view(value);
        return EqualsIgnoreCase(view, "1") ||
               EqualsIgnoreCase(view, "true") ||
               EqualsIgnoreCase(view, "yes") ||
               EqualsIgnoreCase(view, "on");
#endif
    }

    bool HasArgument(int argc, char* argv[], std::string_view expected)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (argv[i] && EqualsIgnoreCase(argv[i], expected))
            {
                return true;
            }
        }
        return false;
    }

    bool ShouldUseNonInteractiveAssertMode(int argc, char* argv[])
    {
        return HasArgument(argc, argv, "--no-assert-dialog") ||
               IsTruthyEnvVar("GLTF_NO_ASSERT_DIALOG") ||
               IsTruthyEnvVar("GLTF_NON_INTERACTIVE_ASSERT");
    }

    void ConfigureNonInteractiveCrashHandling()
    {
#ifdef _WIN32
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

#if defined(_DEBUG)
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
#endif
    }

    bool SyncShaderResourcesForRuntime()
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        const fs::path runtime_dir = fs::current_path(ec);
        if (ec)
        {
            return false;
        }

        const fs::path runtime_shader_dir = runtime_dir / "Resources" / "Shaders";
        const std::vector<fs::path> source_candidates = {
            runtime_dir / ".." / ".." / "RendererDemo" / "Resources" / "Shaders",
            runtime_dir / "RendererDemo" / "Resources" / "Shaders"
        };

        fs::path source_shader_dir;
        for (const fs::path& candidate : source_candidates)
        {
            if (!fs::exists(candidate, ec) || !fs::is_directory(candidate, ec))
            {
                continue;
            }

            if (fs::equivalent(candidate, runtime_shader_dir, ec))
            {
                continue;
            }

            source_shader_dir = candidate;
            break;
        }

        if (source_shader_dir.empty())
        {
            return false;
        }

        fs::create_directories(runtime_shader_dir, ec);
        if (ec)
        {
            return false;
        }

        std::uint32_t copied_file_count = 0;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source_shader_dir, ec))
        {
            if (ec)
            {
                return false;
            }

            if (!entry.is_regular_file(ec))
            {
                continue;
            }

            const fs::path relative_path = fs::relative(entry.path(), source_shader_dir, ec);
            if (ec)
            {
                return false;
            }

            const fs::path target_file = runtime_shader_dir / relative_path;
            fs::create_directories(target_file.parent_path(), ec);
            if (ec)
            {
                return false;
            }

            const bool target_exists = fs::exists(target_file, ec);
            if (ec)
            {
                return false;
            }

            const fs::copy_options copy_mode =
                target_exists ? fs::copy_options::update_existing : fs::copy_options::overwrite_existing;
            if (fs::copy_file(entry.path(), target_file, copy_mode, ec))
            {
                ++copied_file_count;
            }
            if (ec)
            {
                return false;
            }
        }

        std::printf("[INFO] Shader sync complete: %u file(s) updated. source=%s target=%s\n",
                    copied_file_count,
                    source_shader_dir.string().c_str(),
                    runtime_shader_dir.string().c_str());
        return true;
    }
}

int main(int argc, char* argv[])
{
    if (ShouldUseNonInteractiveAssertMode(argc, argv))
    {
        ConfigureNonInteractiveCrashHandling();
        std::printf("[INFO] Non-interactive assert mode enabled.\n");
    }

    if (!SyncShaderResourcesForRuntime())
    {
        std::printf("[WARN] Shader sync skipped or failed.\n");
    }

    if (argc <= 1)
    {
        // No argument so cannot decide run which demo app!
        return 0;
    }

    // Process console arguments
    std::vector<std::string> params(argv + 1, argv + argc);

    std::unique_ptr<DemoBase> demo;
    std::string demo_name = argv[1];
    
    REGISTER_DEMO_APP(demo_name, DemoTriangleApp)
    REGISTER_DEMO_APP(demo_name, DemoAppModelViewer)
    REGISTER_DEMO_APP(demo_name, DemoAppModelViewerFrostedGlass)

    const bool success = demo->Init(params);
    GLTF_CHECK(success);
    
    demo->Run();
    
    return 0;
}
