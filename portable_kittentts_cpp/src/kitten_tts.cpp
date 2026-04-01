#include "kitten_tts.hpp"

#include "wav_writer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#else
#include <dlfcn.h>
#endif

namespace kit
{
    namespace
    {
        constexpr std::uint32_t kSampleRate = 24000;
        constexpr std::size_t kTailTrimSamples = 5000;

        std::string quote_shell(const std::filesystem::path& path)
        {
            std::string s = path.string();
            std::string out = "\"";
            for (char c : s)
            {
                if (c == '"')
                {
                    out += "\\\"";
                }
                else
                {
                    out.push_back(c);
                }
            }
            out.push_back('"');
            return out;
        }

        std::filesystem::path make_temp_file(const std::string& suffix)
        {
            static std::uint64_t counter = 0;
            const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            std::ostringstream name;
            name << "kitten_" << stamp << "_" << counter++ << suffix;
            return std::filesystem::temp_directory_path() / name.str();
        }

        std::string read_pipe(FILE* pipe)
        {
            std::string output;
            char buffer[4096];
            while (std::fgets(buffer, sizeof(buffer), pipe))
            {
                output += buffer;
            }
            return output;
        }

        std::string capture_command(const std::string& command)
        {
#ifdef _WIN32
            FILE* pipe = _popen(command.c_str(), "r");
#else
            FILE* pipe = popen(command.c_str(), "r");
#endif
            if (!pipe)
            {
                throw std::runtime_error("Failed to start phonemizer process.");
            }

            std::string output = read_pipe(pipe);
#ifdef _WIN32
            const int rc = _pclose(pipe);
#else
            const int rc = pclose(pipe);
#endif
            if (rc != 0)
            {
                throw std::runtime_error("Phonemizer process failed.");
            }

            return output;
        }

        std::string to_string_trimmed(std::string s)
        {
            while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
            {
                s.pop_back();
            }
            while (!s.empty() && (s.front() == '\r' || s.front() == '\n' || s.front() == ' ' || s.front() == '\t'))
            {
                s.erase(s.begin());
            }
            return s;
        }

        std::string to_lower_ascii(std::string text)
        {
            for (char& c : text)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return text;
        }

        std::vector<SpeakerChoice> make_speakers(const std::array<float, 8>& priors)
        {
            std::vector<SpeakerChoice> speakers = {
                {"Bella", "expr-voice-2-f", priors[0]},
                {"Jasper", "expr-voice-2-m", priors[1]},
                {"Luna",   "expr-voice-3-f", priors[2]},
                {"Bruno",  "expr-voice-3-m", priors[3]},
                {"Rosie",  "expr-voice-4-f", priors[4]},
                {"Hugo",   "expr-voice-4-m", priors[5]},
                {"Kiki",   "expr-voice-5-f", priors[6]},
                {"Leo",    "expr-voice-5-m", priors[7]},
            };
            return speakers;
        }

        struct ModelSpec
        {
            std::filesystem::path model_root;
            std::string model_name;
            std::vector<SpeakerChoice> speakers;
        };

        ModelSpec resolve_model_spec(const std::filesystem::path& app_root, const std::string& model_variant)
        {
            const std::string model_key = to_lower_ascii(model_variant);
            if (model_key == "nano-int8" || model_key == "kitten-nano-int8" || model_key == "kitten-tts-nano-0.8-int8")
            {
                return {
                    app_root / "model" / "kitten-nano-int8-v0_8-onnx",
                    "KittenML/kitten-tts-nano-0.8-int8",
                    make_speakers({0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.9f, 0.8f, 0.8f}),
                };
            }
            if (model_key == "mini" || model_key == "kitten-mini" || model_key == "kitten-tts-mini" || model_key == "kitten-tts-mini-0.8")
            {
                return {
                    app_root / "model" / "kitten-mini-v0_8-onnx",
                    "KittenML/kitten-tts-mini-0.8",
                    make_speakers({1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}),
                };
            }
            if (model_key == "micro" || model_key == "kitten-micro" || model_key == "kitten-tts-micro" || model_key == "kitten-tts-micro-0.8")
            {
                return {
                    app_root / "model" / "kitten-micro-v0_8-onnx",
                    "KittenML/kitten-tts-micro-0.8",
                    make_speakers({1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}),
                };
            }

            if (model_key == "nano" || model_key == "kitten" || model_key == "kitten-nano" || model_key == "kitten-tts-nano" || model_key == "kitten-tts-nano-0.8" || model_key == "kitten-tts-nano-0.8-fp32")
            {
                return {
                    app_root / "model" / "kitten-nano-v0_8-onnx",
                    "KittenML/kitten-tts-nano-0.8-fp32",
                    make_speakers({0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.9f, 0.8f, 0.8f}),
                };
            }

            throw std::runtime_error(
                "Unknown model: " + model_variant +
                " (use nano, nano-int8, micro, mini, kitten, kitten-nano, kitten-micro, kitten-mini, or kitten-nano-int8)");
        }

#ifdef _WIN32
        std::wstring quote_windows(const std::filesystem::path& path)
        {
            std::wstring s = path.wstring();
            std::wstring out = L"\"";
            for (wchar_t c : s)
            {
                if (c == L'"')
                {
                    out += L"\\\"";
                }
                else
                {
                    out.push_back(c);
                }
            }
            out.push_back(L'"');
            return out;
        }

        std::string capture_process_output(
            const std::filesystem::path& exe,
            const std::vector<std::wstring>& args)
        {
            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = nullptr;

            HANDLE read_pipe = nullptr;
            HANDLE write_pipe = nullptr;
            if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0))
            {
                throw std::runtime_error("Failed to create stdout pipe.");
            }
            if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0))
            {
                CloseHandle(read_pipe);
                CloseHandle(write_pipe);
                throw std::runtime_error("Failed to configure stdout pipe.");
            }

            std::wstring command_line = quote_windows(exe);
            for (const auto& arg : args)
            {
                command_line.push_back(L' ');
                command_line += arg;
            }

            STARTUPINFOW startup_info{};
            startup_info.cb = sizeof(startup_info);
            startup_info.dwFlags = STARTF_USESTDHANDLES;
            startup_info.hStdOutput = write_pipe;
            startup_info.hStdError = write_pipe;
            startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            PROCESS_INFORMATION process_info{};
            std::wstring mutable_command_line = command_line;
            const BOOL started = CreateProcessW(
                exe.c_str(),
                mutable_command_line.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &startup_info,
                &process_info);

            CloseHandle(write_pipe);
            if (!started)
            {
                CloseHandle(read_pipe);
                throw std::runtime_error("Failed to start phonemizer process.");
            }

            std::string output;
            char buffer[4096];
            DWORD bytes_read = 0;
            while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0)
            {
                output.append(buffer, buffer + bytes_read);
            }

            WaitForSingleObject(process_info.hProcess, INFINITE);
            DWORD exit_code = 0;
            GetExitCodeProcess(process_info.hProcess, &exit_code);

            CloseHandle(process_info.hThread);
            CloseHandle(process_info.hProcess);
            CloseHandle(read_pipe);

            if (exit_code != 0)
            {
                throw std::runtime_error("Phonemizer process failed.");
            }

            return output;
        }
#endif

        std::filesystem::path onnxruntime_library_path(const std::filesystem::path& app_root)
        {
#ifdef _WIN32
            return app_root / "runtime" / "onnxruntime" / "onnxruntime.dll";
#elif defined(__APPLE__)
            return app_root / "runtime" / "onnxruntime" / "libonnxruntime.dylib";
#else
            return app_root / "runtime" / "onnxruntime" / "libonnxruntime.so";
#endif
        }

        std::shared_ptr<void> load_shared_library(const std::filesystem::path& path)
        {
#ifdef _WIN32
            HMODULE module = LoadLibraryW(path.c_str());
            if (!module)
            {
                throw std::runtime_error("Failed to load ONNX Runtime: " + path.string());
            }
            return std::shared_ptr<void>(module, [](void* handle)
            {
                if (handle)
                {
                    FreeLibrary(static_cast<HMODULE>(handle));
                }
            });
#else
            const std::string native_path = path.string();
            void* module = dlopen(native_path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!module)
            {
                throw std::runtime_error(std::string("Failed to load ONNX Runtime: ") + dlerror());
            }
            return std::shared_ptr<void>(module, [](void* handle)
            {
                if (handle)
                {
                    dlclose(handle);
                }
            });
#endif
        }

        template <typename Fn>
        Fn load_symbol(void* module, const char* name)
        {
#ifdef _WIN32
            auto* symbol = GetProcAddress(static_cast<HMODULE>(module), name);
#else
            auto* symbol = dlsym(module, name);
#endif
            if (!symbol)
            {
                throw std::runtime_error(std::string("Missing symbol: ") + name);
            }
            return reinterpret_cast<Fn>(symbol);
        }

        std::filesystem::path espeak_library_path(const std::filesystem::path& app_root)
        {
#ifdef _WIN32
            return app_root / "runtime" / "espeak-lite" / "libespeak-ng.dll";
#elif defined(__APPLE__)
            return app_root / "runtime" / "espeak-lite" / "libespeak-ng.dylib";
#else
            return app_root / "runtime" / "espeak-lite" / "libespeak-ng.so";
#endif
        }

        std::filesystem::path espeak_data_path(const std::filesystem::path& app_root)
        {
            return app_root / "runtime" / "espeak-lite" / "espeak-ng-data";
        }

        void set_espeak_data_env(const std::filesystem::path& data_path)
        {
#ifdef _WIN32
            _putenv_s("ESPEAK_DATA_PATH", data_path.string().c_str());
#else
            setenv("ESPEAK_DATA_PATH", data_path.c_str(), 1);
#endif
        }

        std::string strip_trailing_punctuation(const std::string& text)
        {
            std::string punct;
            for (auto it = text.rbegin(); it != text.rend(); ++it)
            {
                const char c = *it;
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    continue;
                }
                if (c == ',' || c == ';' || c == ':' || c == '.' || c == '!' || c == '?')
                {
                    punct.push_back(c);
                    continue;
                }
                break;
            }
            std::reverse(punct.begin(), punct.end());
            return punct;
        }

        const OrtApiBase* get_ort_api_base(void* module)
        {
#ifdef _WIN32
            using GetApiBaseFn = const OrtApiBase* (ORT_API_CALL*)(void);
            auto* get_api_base = reinterpret_cast<GetApiBaseFn>(GetProcAddress(static_cast<HMODULE>(module), "OrtGetApiBase"));
            if (!get_api_base)
            {
                throw std::runtime_error("ONNX Runtime is missing the OrtGetApiBase export.");
            }
            const OrtApiBase* base = get_api_base();
#else
            using GetApiBaseFn = const OrtApiBase* (*)(void);
            auto* get_api_base = reinterpret_cast<GetApiBaseFn>(dlsym(module, "OrtGetApiBase"));
            if (!get_api_base)
            {
                throw std::runtime_error(std::string("ONNX Runtime is missing the OrtGetApiBase export: ") + dlerror());
            }
            const OrtApiBase* base = get_api_base();
#endif
            if (!base)
            {
                throw std::runtime_error("ONNX Runtime returned a null API base.");
            }
            return base;
        }

        std::shared_ptr<void> initialize_onnxruntime(const std::filesystem::path& app_root)
        {
            const auto library = load_shared_library(onnxruntime_library_path(app_root));
            const OrtApiBase* base = get_ort_api_base(library.get());
            // Kitten TTS v0.8's official Windows wheel is built against ONNX Runtime 1.22.1.
            // Using the same API level keeps our portable host aligned with the reference build.
            Ort::InitApi(base->GetApi(22));
            return library;
        }

        bool debug_enabled()
        {
            const char* value = std::getenv("KITTEN_TTS_DEBUG");
            return value && *value && std::string(value) != "0";
        }

        std::vector<std::string> split_words(const std::string& text)
        {
            std::vector<std::string> words;
            std::string current;
            for (char c : text)
            {
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    if (!current.empty())
                    {
                        words.push_back(current);
                        current.clear();
                    }
                }
                else
                {
                    current.push_back(c);
                }
            }
            if (!current.empty())
            {
                words.push_back(current);
            }
            return words;
        }

        bool is_basic_english_whitespace(char32_t cp)
        {
            switch (cp)
            {
            case U' ': case U'\t': case U'\n': case U'\r': case U'\f': case U'\v':
                return true;
            default:
                return false;
            }
        }

        bool is_basic_english_punctuation(char32_t cp)
        {
            return cp == U'!'
                || cp == U'"'
                || cp == U'#'
                || cp == U'$'
                || cp == U'%'
                || cp == U'&'
                || cp == U'\''
                || cp == U'('
                || cp == U')'
                || cp == U'*'
                || cp == U'+'
                || cp == U','
                || cp == U'-'
                || cp == U'.'
                || cp == U'/'
                || cp == U':'
                || cp == U';'
                || cp == U'<'
                || cp == U'='
                || cp == U'>'
                || cp == U'?'
                || cp == U'@'
                || cp == U'['
                || cp == U'\\'
                || cp == U']'
                || cp == U'^'
                || cp == U'`'
                || cp == U'{'
                || cp == U'|'
                || cp == U'}'
                || cp == U'~';
        }

        std::string basic_english_tokenize_join(const std::string& text)
        {
            std::vector<std::string> tokens;
            std::u32string current;

            auto flush_current = [&]()
            {
                if (!current.empty())
                {
                    tokens.emplace_back(u32_to_utf8(current));
                    current.clear();
                }
            };

            for (char32_t cp : utf8_to_u32(text))
            {
                if (is_basic_english_whitespace(cp))
                {
                    flush_current();
                }
                else if (is_basic_english_punctuation(cp))
                {
                    flush_current();
                    tokens.emplace_back(u32_to_utf8(std::u32string(1, cp)));
                }
                else
                {
                    current.push_back(cp);
                }
            }

            flush_current();

            std::ostringstream out;
            for (std::size_t i = 0; i < tokens.size(); ++i)
            {
                if (i)
                {
                    out << ' ';
                }
                out << tokens[i];
            }
            return out.str();
        }
    }

    KittenTtsEngine::KittenTtsEngine(
        std::shared_ptr<void> onnxruntime_module,
        std::shared_ptr<void> espeak_module,
        KittenTtsEngine::EspeakInitializeFn espeak_initialize,
        KittenTtsEngine::EspeakTerminateFn espeak_terminate,
        KittenTtsEngine::EspeakSetVoiceByNameFn espeak_set_voice_by_name,
        KittenTtsEngine::EspeakTextToPhonemesFn espeak_text_to_phonemes,
        Ort::Env env,
        Ort::Session session,
        IpaTokenizer tokenizer,
        std::unordered_map<std::string, VoiceInfo> voices,
        std::vector<SpeakerChoice> speakers,
        std::unordered_map<std::string, SpeakerChoice> speaker_lookup,
        std::filesystem::path espeak_root,
        std::string model_name)
        : _onnxruntime_module(std::move(onnxruntime_module))
        , _espeak_module(std::move(espeak_module))
        , _espeak_initialize(espeak_initialize)
        , _espeak_terminate(espeak_terminate)
        , _espeak_set_voice_by_name(espeak_set_voice_by_name)
        , _espeak_text_to_phonemes(espeak_text_to_phonemes)
        , _env(std::move(env))
        , _session(std::move(session))
        , _tokenizer(std::move(tokenizer))
        , _voices(std::move(voices))
        , _speakers(std::move(speakers))
        , _speaker_lookup(std::move(speaker_lookup))
        , _espeak_root(std::move(espeak_root))
        , _model_name(std::move(model_name))
    {
    }

    KittenTtsEngine KittenTtsEngine::load(const std::filesystem::path& app_root, const std::string& model_variant)
    {
        const ModelSpec model = resolve_model_spec(app_root, model_variant);
        const auto model_root = model.model_root;
        const auto espeak_root = app_root / "runtime" / "espeak-lite";
        const auto espeak_data = espeak_root / "espeak-ng-data";
        const auto espeak_library = espeak_library_path(app_root);
        const auto model_path = model_root / "onnx" / "model.onnx";
        const auto voices_dir = model_root / "voices";
        const auto symbols_path = app_root / "data" / "ipa_symbols.txt";

        if (!std::filesystem::exists(model_path))
        {
            throw std::runtime_error("Missing model: " + model_path.string());
        }
        if (!std::filesystem::exists(espeak_data))
        {
            throw std::runtime_error("Missing eSpeak data directory: " + espeak_data.string());
        }
        if (!std::filesystem::exists(espeak_library))
        {
            throw std::runtime_error("Missing eSpeak runtime: " + espeak_library.string());
        }
        if (!std::filesystem::exists(voices_dir))
        {
            throw std::runtime_error("Missing voice directory: " + voices_dir.string());
        }
        if (!std::filesystem::exists(symbols_path))
        {
            throw std::runtime_error("Missing symbol table: " + symbols_path.string());
        }

        const std::vector<SpeakerChoice> speakers = model.speakers;

        std::unordered_map<std::string, SpeakerChoice> lookup;
        for (const auto& speaker : speakers)
        {
            lookup.emplace(speaker.alias, speaker);
            lookup.emplace(speaker.voice_id, speaker);
        }
        lookup.emplace("male", lookup.at("Jasper"));
        lookup.emplace("female", lookup.at("Bella"));

        const auto voice_ids = std::vector<std::string>{
            "expr-voice-2-f",
            "expr-voice-2-m",
            "expr-voice-3-f",
            "expr-voice-3-m",
            "expr-voice-4-f",
            "expr-voice-4-m",
            "expr-voice-5-f",
            "expr-voice-5-m",
        };
        auto voices = load_voices(voices_dir, voice_ids);

        auto onnxruntime_module = initialize_onnxruntime(app_root);
        auto espeak_module = load_shared_library(espeak_library);
        auto espeak_initialize = load_symbol<KittenTtsEngine::EspeakInitializeFn>(espeak_module.get(), "espeak_Initialize");
        auto espeak_terminate = load_symbol<KittenTtsEngine::EspeakTerminateFn>(espeak_module.get(), "espeak_Terminate");
        auto espeak_set_voice_by_name = load_symbol<KittenTtsEngine::EspeakSetVoiceByNameFn>(espeak_module.get(), "espeak_SetVoiceByName");
        auto espeak_text_to_phonemes = load_symbol<KittenTtsEngine::EspeakTextToPhonemesFn>(espeak_module.get(), "espeak_TextToPhonemes");

        set_espeak_data_env(espeak_data);
        if (espeak_initialize(2, 0, espeak_data.string().c_str(), 0) <= 0)
        {
            throw std::runtime_error("Failed to initialize eSpeak phonemizer.");
        }
        if (espeak_set_voice_by_name("en-us") != 0)
        {
            throw std::runtime_error("Failed to load eSpeak voice en-us.");
        }

        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "kitten-tts");
        Ort::SessionOptions session_options;
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session_options.SetIntraOpNumThreads(1);
        session_options.SetInterOpNumThreads(1);
        Ort::Session session(env, model_path.c_str(), session_options);
        IpaTokenizer tokenizer(symbols_path);

        return KittenTtsEngine(
            std::move(onnxruntime_module),
            std::move(espeak_module),
            espeak_initialize,
            espeak_terminate,
            espeak_set_voice_by_name,
            espeak_text_to_phonemes,
            std::move(env),
            std::move(session),
            std::move(tokenizer),
            std::move(voices),
            speakers,
            std::move(lookup),
            espeak_root,
            model.model_name);
    }

    KittenTtsEngine::~KittenTtsEngine()
    {
        if (_espeak_terminate)
        {
            _espeak_terminate();
        }
    }

    void KittenTtsEngine::print_speakers() const
    {
        std::cout << speakers_text();
    }

    std::string KittenTtsEngine::speakers_text() const
    {
        std::ostringstream out;
        out << _model_name << " voices:\n";
        for (std::size_t i = 0; i < _speakers.size(); ++i)
        {
            const auto& s = _speakers[i];
            out << i << ": " << s.alias << " (" << s.voice_id << ")";
            if (std::fabs(s.speed_prior - 1.0f) > 0.0001f)
            {
                out << " speed-prior=" << s.speed_prior;
            }
            out << "\n";
        }
        out << "Aliases: male -> Jasper, female -> Bella\n";
        return out.str();
    }

    SpeakerChoice KittenTtsEngine::resolve_speaker(const std::string& speaker) const
    {
        try
        {
            std::size_t idx = 0;
            const int parsed = std::stoi(speaker, &idx);
            if (idx == speaker.size())
            {
                if (parsed >= 0 && static_cast<std::size_t>(parsed) < _speakers.size())
                {
                    return _speakers[static_cast<std::size_t>(parsed)];
                }
                throw std::runtime_error("Speaker index out of range: " + speaker);
            }
        }
        catch (...)
        {
        }

        auto it = _speaker_lookup.find(speaker);
        if (it != _speaker_lookup.end())
        {
            return it->second;
        }

        throw std::runtime_error("Unknown speaker: " + speaker);
    }

    std::string KittenTtsEngine::phonemize_with_espeak(const std::string& text) const
    {
        if (text.empty())
        {
            return {};
        }

        if (!_espeak_text_to_phonemes)
        {
            throw std::runtime_error("eSpeak phonemizer is not initialized.");
        }

        std::vector<char> buffer(text.begin(), text.end());
        buffer.push_back('\0');
        char* cursor = buffer.data();
        char** text_ptr = &cursor;

        constexpr int text_mode = 1;
        constexpr int phonemes_mode = (static_cast<int>('_') << 8) | 0x02;

        std::string output;
        bool first = true;
        while (*text_ptr && **text_ptr)
        {
            const char* phonemes = _espeak_text_to_phonemes(text_ptr, text_mode, phonemes_mode);
            if (phonemes && *phonemes)
            {
                if (!first)
                {
                    output.push_back(' ');
                }
                output += phonemes;
                first = false;
            }
            else
            {
                break;
            }
        }

        output.erase(std::remove(output.begin(), output.end(), '_'), output.end());
        output = to_string_trimmed(std::move(output));

        const std::string trailing_punct = strip_trailing_punctuation(text);
        if (!trailing_punct.empty())
        {
            if (output.size() < trailing_punct.size() || output.substr(output.size() - trailing_punct.size()) != trailing_punct)
            {
                output += trailing_punct;
            }
        }

        return output;
    }

    std::vector<float> KittenTtsEngine::synthesize_chunk(const std::string& chunk_text, const SpeakerChoice& speaker, float speed)
    {
        const std::string phonemes_raw = phonemize_with_espeak(chunk_text);
        const std::string phonemes = basic_english_tokenize_join(phonemes_raw);

        auto tokens = _tokenizer.tokenize(phonemes);
        if (tokens.empty())
        {
            return {};
        }

        const auto voice_it = _voices.find(speaker.voice_id);
        if (voice_it == _voices.end())
        {
            throw std::runtime_error("Voice missing: " + speaker.voice_id);
        }
        const VoiceInfo& voice = voice_it->second;
        const std::size_t ref_id = std::min<std::size_t>(chunk_text.size(), voice.rows > 0 ? voice.rows - 1 : 0);
        const std::size_t style_dim = voice.cols;
        std::vector<float> style(style_dim);
        const std::size_t style_offset = ref_id * style_dim;
        std::copy_n(voice.data.begin() + static_cast<std::ptrdiff_t>(style_offset), static_cast<std::ptrdiff_t>(style_dim), style.begin());

        float effective_speed = speed;
        if (speaker.speed_prior > 0.0f)
        {
            effective_speed *= speaker.speed_prior;
        }
        const std::size_t word_count = split_words(chunk_text).size();

        if (debug_enabled())
        {
            std::cerr << "[kitten-debug] chunk='" << chunk_text << "'\n";
            std::cerr << "[kitten-debug] phonemes='" << phonemes << "'\n";
            std::cerr << "[kitten-debug] tokens=" << tokens.size() << " ref_id=" << ref_id << " style_dim=" << style_dim << " words=" << word_count << " speed=" << speed << " effective=" << effective_speed << "\n";
            std::cerr << "[kitten-debug] token_ids=";
            for (std::size_t i = 0; i < tokens.size() && i < 32; ++i)
            {
                if (i)
                {
                    std::cerr << ',';
                }
                std::cerr << tokens[i];
            }
            std::cerr << "\n";
            std::cerr << "[kitten-debug] style0=";
            for (std::size_t i = 0; i < style.size() && i < 8; ++i)
            {
                if (i)
                {
                    std::cerr << ',';
                }
                std::cerr << style[i];
            }
            std::cerr << "\n";
        }

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<std::int64_t> input_shape{1, static_cast<std::int64_t>(tokens.size())};
        std::vector<std::int64_t> style_shape{1, static_cast<std::int64_t>(style_dim)};
        std::vector<std::int64_t> speed_shape{1};

        Ort::Value input_ids_tensor = Ort::Value::CreateTensor<std::int64_t>(memory_info, tokens.data(), tokens.size(), input_shape.data(), input_shape.size());
        Ort::Value style_tensor = Ort::Value::CreateTensor<float>(memory_info, style.data(), style.size(), style_shape.data(), style_shape.size());
        Ort::Value speed_tensor = Ort::Value::CreateTensor<float>(memory_info, &effective_speed, 1, speed_shape.data(), speed_shape.size());

        const char* input_names[] = {"input_ids", "style", "speed"};
        const char* output_names[] = {"waveform", "duration"};
        Ort::Value input_values[] = {std::move(input_ids_tensor), std::move(style_tensor), std::move(speed_tensor)};

        auto outputs = _session.Run(Ort::RunOptions{nullptr}, input_names, input_values, 3, output_names, 2);
        auto& waveform = outputs[0];
        auto& duration = outputs[1];
        auto type_info = waveform.GetTensorTypeAndShapeInfo();
        const std::size_t sample_count = type_info.GetElementCount();
        const float* data = waveform.GetTensorData<float>();
        std::vector<float> samples(data, data + sample_count);

        if (debug_enabled())
        {
            std::cerr << "[kitten-debug] raw_samples=" << sample_count << "\n";
            auto duration_info = duration.GetTensorTypeAndShapeInfo();
            const std::size_t duration_count = duration_info.GetElementCount();
            if (duration_count > 0)
            {
                std::cerr << "[kitten-debug] duration=";
                const auto* duration_data = duration.GetTensorData<std::int64_t>();
                for (std::size_t i = 0; i < duration_count; ++i)
                {
                    if (i)
                    {
                        std::cerr << ',';
                    }
                    std::cerr << duration_data[i];
                }
                std::cerr << "\n";
            }
        }

        if (samples.size() > 24000)
        {
            if (samples.size() > kTailTrimSamples)
            {
                samples.resize(samples.size() - kTailTrimSamples);
            }
        }
        else if (debug_enabled())
        {
            std::cerr << "[kitten-debug] trim_skipped\n";
        }
        return samples;
    }

    AudioResult KittenTtsEngine::synthesize(const std::string& text, const std::string& speaker, float speed, bool clean_text)
    {
        const SpeakerChoice choice = resolve_speaker(speaker);
        const std::string cleaned = clean_text ? preprocess_text(text) : text;
        const auto chunks = chunk_text(cleaned);

        AudioResult result;
        result.sample_rate = kSampleRate;
        result.voice_display_name = choice.alias;

        std::vector<float> all_samples;
        for (const auto& chunk : chunks)
        {
            const auto chunk_samples = synthesize_chunk(chunk, choice, speed);
            all_samples.insert(all_samples.end(), chunk_samples.begin(), chunk_samples.end());
        }
        result.samples = std::move(all_samples);
        return result;
    }
}
