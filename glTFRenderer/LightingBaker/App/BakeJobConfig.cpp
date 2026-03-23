#include "BakeJobConfig.h"

#include <sstream>
#include <string_view>

namespace LightingBaker
{
    namespace
    {
        bool TryParseUnsigned(std::wstring_view value, unsigned& out_value)
        {
            try
            {
                std::size_t parsed = 0;
                const unsigned long converted = std::stoul(std::wstring(value), &parsed, 10);
                if (parsed != value.size())
                {
                    return false;
                }

                out_value = static_cast<unsigned>(converted);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool RequireNextValue(int argc, wchar_t* argv[], int& index, std::wstring& out_value, std::wstring& out_error)
        {
            if ((index + 1) >= argc)
            {
                out_error = L"Missing value for argument: " + std::wstring(argv[index]);
                return false;
            }

            out_value = argv[++index];
            return true;
        }
    }

    CommandLineParseResult ParseCommandLine(int argc, wchar_t* argv[])
    {
        CommandLineParseResult result{};

        for (int index = 1; index < argc; ++index)
        {
            const std::wstring_view arg = argv[index];

            if (arg == L"--help" || arg == L"-h")
            {
                result.config.show_help = true;
                continue;
            }

            if (arg == L"--scene")
            {
                std::wstring value;
                if (!RequireNextValue(argc, argv, index, value, result.error_message))
                {
                    return result;
                }

                result.config.scene_path = value;
                continue;
            }

            if (arg == L"--output")
            {
                std::wstring value;
                if (!RequireNextValue(argc, argv, index, value, result.error_message))
                {
                    return result;
                }

                result.config.output_root = value;
                continue;
            }

            if (arg == L"--resolution")
            {
                std::wstring value;
                if (!RequireNextValue(argc, argv, index, value, result.error_message))
                {
                    return result;
                }

                if (!TryParseUnsigned(value, result.config.atlas_resolution) || result.config.atlas_resolution == 0)
                {
                    result.error_message = L"Invalid --resolution value: " + value;
                    return result;
                }
                continue;
            }

            if (arg == L"--samples-per-iteration")
            {
                std::wstring value;
                if (!RequireNextValue(argc, argv, index, value, result.error_message))
                {
                    return result;
                }

                if (!TryParseUnsigned(value, result.config.samples_per_iteration) || result.config.samples_per_iteration == 0)
                {
                    result.error_message = L"Invalid --samples-per-iteration value: " + value;
                    return result;
                }
                continue;
            }

            if (arg == L"--target-samples")
            {
                std::wstring value;
                if (!RequireNextValue(argc, argv, index, value, result.error_message))
                {
                    return result;
                }

                if (!TryParseUnsigned(value, result.config.target_samples) || result.config.target_samples == 0)
                {
                    result.error_message = L"Invalid --target-samples value: " + value;
                    return result;
                }
                continue;
            }

            if (arg == L"--max-bounces")
            {
                std::wstring value;
                if (!RequireNextValue(argc, argv, index, value, result.error_message))
                {
                    return result;
                }

                if (!TryParseUnsigned(value, result.config.max_bounces))
                {
                    result.error_message = L"Invalid --max-bounces value: " + value;
                    return result;
                }
                continue;
            }

            if (arg == L"--no-progressive")
            {
                result.config.progressive = false;
                continue;
            }

            if (arg == L"--resume")
            {
                result.config.resume = true;
                continue;
            }

            result.error_message = L"Unknown argument: " + std::wstring(arg);
            return result;
        }

        if (result.config.show_help)
        {
            return result;
        }

        if (result.config.scene_path.empty())
        {
            result.error_message = L"Missing required --scene argument.";
            return result;
        }

        if (result.config.output_root.empty())
        {
            const std::wstring scene_stem = result.config.scene_path.stem().native();
            result.config.output_root = result.config.scene_path.parent_path() / (scene_stem + L".lmbake");
        }

        return result;
    }

    std::wstring BuildUsageText()
    {
        std::wostringstream stream;
        stream
            << L"LightingBaker Phase A Scaffold\n"
            << L"Usage:\n"
            << L"  LightingBaker.exe --scene <scene.gltf> [options]\n\n"
            << L"Options:\n"
            << L"  --output <dir>                 Output root, default: <scene>.lmbake\n"
            << L"  --resolution <n>              Atlas resolution, default: 1024\n"
            << L"  --samples-per-iteration <n>   Progressive samples per iteration, default: 1\n"
            << L"  --target-samples <n>          Total target samples, default: 256\n"
            << L"  --max-bounces <n>             Maximum diffuse bounces, default: 4\n"
            << L"  --resume                      Resume from existing bake cache\n"
            << L"  --no-progressive              Disable progressive mode for diagnostics\n"
            << L"  --help, -h                    Show this help text\n";
        return stream.str();
    }
}
