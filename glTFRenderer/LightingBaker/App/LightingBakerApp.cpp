#include "LightingBakerApp.h"

#include "Output/BakeOutputWriter.h"

#include <iostream>

namespace LightingBaker
{
    namespace
    {
        constexpr int kExitSuccess = 0;
        constexpr int kExitInvalidArguments = 1;
        constexpr int kExitFilesystemError = 2;
        constexpr int kExitOutputWriteError = 3;
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
        BakeOutputWriter output_writer{};
        const BakeOutputLayout output_layout = BakeOutputWriter::BuildLayout(config.output_root);

        std::wstring error_message;
        if (!output_writer.EnsureLayout(output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitFilesystemError;
        }

        if (!output_writer.WriteBootstrapPackage(config, output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        PrintResolvedJob(config, output_layout);

        std::wcout
            << L"\nPhase A scaffold is active. Bootstrap sidecar package written.\n"
            << L"  manifest: " << output_layout.manifest_path.native() << L"\n"
            << L"  resume metadata: " << output_layout.resume_path.native() << L"\n"
            << L"\nPlanned pipeline:\n"
            << L"  1. Initialize maintained runtime host\n"
            << L"  2. Import scene via BakeSceneImporter\n"
            << L"  3. Validate UV1 and atlas layout\n"
            << L"  4. Build atlas-domain bake records\n"
            << L"  5. Execute path-traced bake passes\n"
            << L"  6. Accumulate progressive results and write sidecar outputs\n";

        return kExitSuccess;
    }

    void LightingBakerApp::PrintResolvedJob(const BakeJobConfig& config, const BakeOutputLayout& output_layout) const
    {
        std::wcout
            << L"LightingBaker job\n"
            << L"  scene: " << config.scene_path.native() << L"\n"
            << L"  output: " << output_layout.root.native() << L"\n"
            << L"  manifest: " << output_layout.manifest_path.native() << L"\n"
            << L"  resume metadata: " << output_layout.resume_path.native() << L"\n"
            << L"  atlas resolution: " << config.atlas_resolution << L"\n"
            << L"  samples / iteration: " << config.samples_per_iteration << L"\n"
            << L"  target samples: " << config.target_samples << L"\n"
            << L"  max bounces: " << config.max_bounces << L"\n"
            << L"  progressive: " << (config.progressive ? L"true" : L"false") << L"\n"
            << L"  resume: " << (config.resume ? L"true" : L"false") << L"\n";
    }
}
