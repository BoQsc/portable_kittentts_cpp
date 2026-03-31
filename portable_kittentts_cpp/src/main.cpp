#include "audio_player.hpp"
#include "kitten_tts.hpp"
#include "wav_writer.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    struct CliOptions
    {
        bool help = false;
        bool list_speakers = false;
        std::string model = "nano";
        std::string speaker = "male";
        std::string text;
        std::filesystem::path output = "infer_outputs/output.wav";
        float speed = 1.0f;
        std::string device = "cpu";
        bool clean_text = false;
    };

    std::string trim_ascii(std::string value)
    {
        auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
        while (!value.empty() && is_space(static_cast<unsigned char>(value.front())))
        {
            value.erase(value.begin());
        }
        while (!value.empty() && is_space(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }
        return value;
    }

    std::filesystem::path executable_dir()
    {
#ifdef _WIN32
        wchar_t buffer[32768];
        const DWORD len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
        if (len == 0 || len >= sizeof(buffer) / sizeof(buffer[0]))
        {
            throw std::runtime_error("Failed to resolve executable path.");
        }
        return std::filesystem::path(buffer).parent_path();
#else
        return std::filesystem::current_path();
#endif
    }

    std::filesystem::path resolve_app_root()
    {
        if (const char* env = std::getenv("KITTEN_TTS_ROOT"))
        {
            if (*env)
            {
                return std::filesystem::path(env);
            }
        }
        return executable_dir();
    }

    std::filesystem::path resolve_output_root(const std::filesystem::path& app_root)
    {
        if (const char* env = std::getenv("KITTEN_TTS_OUTPUT_ROOT"))
        {
            if (*env)
            {
                return std::filesystem::path(env);
            }
        }
        return app_root;
    }

    CliOptions parse_args(int argc, char** argv)
    {
        CliOptions opts;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            auto next_value = [&](const char* name) -> std::string
            {
                if (i + 1 >= argc)
                {
                    throw std::runtime_error(std::string("Missing value for ") + name);
                }
                return std::string(argv[++i]);
            };

            if (arg == "--help" || arg == "-h" || arg == "/?")
            {
                opts.help = true;
            }
            else if (arg == "--list-speakers")
            {
                opts.list_speakers = true;
            }
            else if (arg == "--speaker")
            {
                opts.speaker = next_value("--speaker");
            }
            else if (arg == "--speed")
            {
                opts.speed = std::stof(next_value("--speed"));
            }
            else if (arg == "--text")
            {
                opts.text = next_value("--text");
            }
            else if (arg == "--output")
            {
                opts.output = next_value("--output");
            }
            else if (arg == "--model")
            {
                opts.model = next_value("--model");
            }
            else if (arg == "--device")
            {
                opts.device = next_value("--device");
            }
            else if (arg == "--clean-text")
            {
                opts.clean_text = true;
            }
            else if (!arg.empty() && arg[0] != '-')
            {
                if (opts.text.empty())
                {
                    opts.text = arg;
                }
                else
                {
                    opts.text.append(" ");
                    opts.text.append(arg);
                }
            }
            else
            {
                throw std::runtime_error("Unknown argument: " + arg);
            }
        }
        return opts;
    }

    void print_help()
    {
        std::cout
            << "Kitten TTS portable C++ CLI\n"
            << "Usage:\n"
            << "  kitten_tts --text \"Hello world\" [--speaker male] [--speed 1.0] [--output infer_outputs/output.wav]\n"
            << "  kitten_tts --list-speakers\n"
            << "  kitten_tts            # interactive mode\n\n"
            << "Options:\n"
            << "  --speaker NAME|INDEX   Bella, Jasper, Luna, Bruno, Rosie, Hugo, Kiki, Leo\n"
            << "  --speaker male|female  Compatibility aliases\n"
            << "  --speed FLOAT          Speech speed, default 1.0\n"
            << "  --device cpu           Accepted for compatibility; CPU only in this build\n"
            << "  --model nano|nano-int8|micro|mini Select the bundled model\n"
            << "  --clean-text           Enable number / currency expansion before phonemizing\n"
            << "  --output FILE.wav      WAV output path\n"
            << "  --list-speakers        Print the voice list\n";
    }

    void synthesize_once(kit::KittenTtsEngine& engine, const CliOptions& opts)
    {
        const auto result = engine.synthesize(opts.text, opts.speaker, opts.speed, opts.clean_text);
        if (!opts.output.parent_path().empty())
        {
            std::filesystem::create_directories(opts.output.parent_path());
        }
        kit::write_wav_pcm16(opts.output.string(), result.samples, result.sample_rate);
        std::cout << "Saved " << opts.output.string() << "\n";
        std::cout << "Model " << engine.model_name() << ", voice " << result.voice_display_name << "\n";
        kit::play_wav_file(opts.output);
    }
}

int main(int argc, char** argv)
{
    try
    {
        CliOptions opts = parse_args(argc, argv);
        if (opts.help)
        {
            print_help();
            return 0;
        }

        if (opts.device != "cpu")
        {
            std::cout << "Warning: this build uses CPU inference only; ignoring --device " << opts.device << "\n";
        }

        const auto app_root = resolve_app_root();
        const auto output_root = resolve_output_root(app_root);
        if (opts.output.is_relative())
        {
            opts.output = output_root / opts.output;
        }
        auto engine = kit::KittenTtsEngine::load(app_root, opts.model);

        if (opts.list_speakers)
        {
            engine.print_speakers();
            return 0;
        }

        if (!opts.text.empty())
        {
            synthesize_once(engine, opts);
            return 0;
        }

        std::cout << "Interactive mode. Press Enter on an empty line to exit.\n";
        std::cout << "Default voice: " << opts.speaker << "\n";

        std::string line;
        while (true)
        {
            std::cout << "> " << std::flush;
            if (!std::getline(std::cin, line))
            {
                break;
            }

            const std::string text = trim_ascii(line);
            if (text.empty())
            {
                break;
            }

            CliOptions run_opts = opts;
            run_opts.text = text;
            synthesize_once(engine, run_opts);
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
