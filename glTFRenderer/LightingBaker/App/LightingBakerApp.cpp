#include "LightingBakerApp.h"

#include <filesystem>
#include <iostream>
#include <system_error>

namespace LightingBaker
{
    namespace
    {
        constexpr int kExitSuccess = 0;
        constexpr int kExitInvalidArguments = 1;
        constexpr int kExitFilesystemError = 2;
    }

    int LightingBakerApp::Run(int argc, wchar_t* argv[])
    {
        const CommandLineParseResult parse_result = ParseCommandLine(argc, argv);
        if (!parse_result.error_message.empty())
        {
            std::wcerr << parse_result.error_message << L"\n\n" << BuildUsageText();
            return kExitInvalidArguments;
        }

        if (parse_result.config.show_help)
        {
            std::wcout << BuildUsageText();
            return kExitSuccess;
        }

        return RunJob(parse_result.config);
    }

    int LightingBakerApp::RunJob(const BakeJobConfig& config)
    {
        std::wstring error_message;
        if (!EnsureOutputLayout(config, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitFilesystemError;
        }

        PrintResolvedJob(config);

        std::wcout
            << L"\nPhase A scaffold is active. Planned pipeline:\n"
            << L"  1. Initialize maintained runtime host\n"
            << L"  2. Import scene via BakeSceneImporter\n"
            << L"  3. Validate UV1 and atlas layout\n"
            << L"  4. Build atlas-domain bake records\n"
            << L"  5. Execute path-traced bake passes\n"
            << L"  6. Accumulate progressive results and write sidecar outputs\n";

        return kExitSuccess;
    }

    bool LightingBakerApp::EnsureOutputLayout(const BakeJobConfig& config, std::wstring& out_error) const
    {
        const std::filesystem::path atlas_dir = config.output_root / L"atlases";
        const std::filesystem::path debug_dir = config.output_root / L"debug";
        const std::filesystem::path cache_dir = config.output_root / L"cache";

        std::error_code error_code;
        std::filesystem::create_directories(atlas_dir, error_code);
        if (error_code)
        {
            out_error = L"Failed to create atlas output directory: " + atlas_dir.native();
            return false;
        }

        std::filesystem::create_directories(debug_dir, error_code);
        if (error_code)
        {
            out_error = L"Failed to create debug output directory: " + debug_dir.native();
            return false;
        }

        std::filesystem::create_directories(cache_dir, error_code);
        if (error_code)
        {
            out_error = L"Failed to create cache output directory: " + cache_dir.native();
            return false;
        }

        return true;
    }

    void LightingBakerApp::PrintResolvedJob(const BakeJobConfig& config) const
    {
        std::wcout
            << L"LightingBaker job\n"
            << L"  scene: " << config.scene_path.native() << L"\n"
            << L"  output: " << config.output_root.native() << L"\n"
            << L"  atlas resolution: " << config.atlas_resolution << L"\n"
            << L"  samples / iteration: " << config.samples_per_iteration << L"\n"
            << L"  target samples: " << config.target_samples << L"\n"
            << L"  max bounces: " << config.max_bounces << L"\n"
            << L"  progressive: " << (config.progressive ? L"true" : L"false") << L"\n"
            << L"  resume: " << (config.resume ? L"true" : L"false") << L"\n";
    }
}
