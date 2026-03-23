#include "LightingBakerApp.h"

#include "Bake/Atlas/LightmapAtlasBuilder.h"
#include "Output/BakeOutputWriter.h"
#include "Scene/BakeSceneImporter.h"

#include <iostream>

namespace LightingBaker
{
    namespace
    {
        constexpr int kExitSuccess = 0;
        constexpr int kExitInvalidArguments = 1;
        constexpr int kExitFilesystemError = 2;
        constexpr int kExitOutputWriteError = 3;
        constexpr int kExitSceneImportError = 4;
        constexpr int kExitSceneValidationError = 5;
        constexpr int kExitAtlasBuildError = 6;
        constexpr int kExitAtlasValidationError = 7;

        unsigned CountErrors(const BakeSceneImportResult& import_result)
        {
            unsigned count = static_cast<unsigned>(import_result.errors.size());
            for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
            {
                count += static_cast<unsigned>(primitive.errors.size());
            }

            return count;
        }

        unsigned CountWarnings(const BakeSceneImportResult& import_result)
        {
            unsigned count = static_cast<unsigned>(import_result.warnings.size());
            for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
            {
                count += static_cast<unsigned>(primitive.warnings.size());
            }

            return count;
        }

        unsigned CountErrors(const LightmapAtlasBuildResult& atlas_result)
        {
            return static_cast<unsigned>(atlas_result.errors.size());
        }

        unsigned CountWarnings(const LightmapAtlasBuildResult& atlas_result)
        {
            return static_cast<unsigned>(atlas_result.warnings.size());
        }
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

        BakeSceneImporter scene_importer{};
        BakeSceneImportRequest scene_import_request{};
        scene_import_request.scene_path = config.scene_path;
        BakeSceneImportResult import_result{};
        const bool import_success = scene_importer.ImportScene(scene_import_request, import_result, error_message);
        const std::wstring import_error_message = error_message;

        error_message.clear();
        if (!output_writer.WriteImportSummary(import_result, output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        error_message.clear();
        if (!import_success)
        {
            if (!output_writer.WriteBootstrapPackage(config, output_layout, import_result, nullptr, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitOutputWriteError;
            }

            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            std::wcerr << import_error_message << L"\n";
            return kExitSceneImportError;
        }

        if (import_result.HasValidationErrors())
        {
            if (!output_writer.WriteBootstrapPackage(config, output_layout, import_result, nullptr, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitOutputWriteError;
            }

            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            std::wcerr << L"Scene validation failed. See import summary for details.\n";
            return kExitSceneValidationError;
        }

        LightmapAtlasBuilder atlas_builder{};
        LightmapAtlasBuildSettings atlas_settings{};
        atlas_settings.atlas_resolution = config.atlas_resolution;
        LightmapAtlasBuildResult atlas_result{};
        error_message.clear();
        if (!atlas_builder.BuildAtlas(import_result, atlas_settings, atlas_result, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitAtlasBuildError;
        }

        error_message.clear();
        if (!output_writer.WriteAtlasSummary(atlas_result, output_layout, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        if (atlas_result.HasValidationErrors())
        {
            error_message.clear();
            if (!output_writer.WriteBootstrapPackage(config, output_layout, import_result, &atlas_result, error_message))
            {
                std::wcerr << error_message << L"\n";
                return kExitOutputWriteError;
            }

            PrintResolvedJob(config, output_layout);
            PrintImportSummary(import_result);
            PrintAtlasSummary(atlas_result);
            std::wcerr << L"Atlas validation failed. See atlas summary for details.\n";
            return kExitAtlasValidationError;
        }

        error_message.clear();
        if (!output_writer.WriteBootstrapPackage(config, output_layout, import_result, &atlas_result, error_message))
        {
            std::wcerr << error_message << L"\n";
            return kExitOutputWriteError;
        }

        PrintResolvedJob(config, output_layout);
        PrintImportSummary(import_result);
        PrintAtlasSummary(atlas_result);

        const std::filesystem::path import_summary_path = output_layout.debug / L"import_summary.json";
        const std::filesystem::path atlas_summary_path = output_layout.debug / L"atlas_summary.json";
        std::wcout
            << L"\nPhase A scaffold is active. Bootstrap sidecar package written.\n"
            << L"  manifest: " << output_layout.manifest_path.native() << L"\n"
            << L"  resume metadata: " << output_layout.resume_path.native() << L"\n"
            << L"  import summary: " << import_summary_path.native() << L"\n"
            << L"  atlas summary: " << atlas_summary_path.native() << L"\n"
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

    void LightingBakerApp::PrintImportSummary(const BakeSceneImportResult& import_result) const
    {
        std::wcout
            << L"\nScene import summary\n"
            << L"  load succeeded: " << (import_result.load_succeeded ? L"true" : L"false") << L"\n"
            << L"  nodes: " << import_result.node_count << L"\n"
            << L"  meshes: " << import_result.mesh_count << L"\n"
            << L"  instance primitives: " << import_result.instance_primitive_count << L"\n"
            << L"  valid lightmap primitives: " << import_result.valid_lightmap_primitive_count << L"\n"
            << L"  warnings: " << CountWarnings(import_result) << L"\n"
            << L"  errors: " << CountErrors(import_result) << L"\n";
    }

    void LightingBakerApp::PrintAtlasSummary(const LightmapAtlasBuildResult& atlas_result) const
    {
        std::wcout
            << L"\nAtlas build summary\n"
            << L"  atlas resolution: " << atlas_result.atlas_resolution << L"\n"
            << L"  texel border: " << atlas_result.texel_border << L"\n"
            << L"  bindings: " << atlas_result.binding_count << L"\n"
            << L"  texel records: " << atlas_result.texel_record_count << L"\n"
            << L"  overlapped texels: " << atlas_result.overlapped_texel_count << L"\n"
            << L"  warnings: " << CountWarnings(atlas_result) << L"\n"
            << L"  errors: " << CountErrors(atlas_result) << L"\n";
    }
}
