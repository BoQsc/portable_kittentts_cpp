#include "audio_player.hpp"
#include "cli_help.hpp"
#include "kitten_tts.hpp"
#include "wav_writer.hpp"

#include <cctype>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

namespace
{
    struct CliOptions
    {
        bool help = false;
        bool list_speakers = false;
        bool session = false;
        bool send = false;
        bool terminate = false;
        bool session_server = false;
        std::string model = "nano";
        std::string session_name;
        std::string speaker = "male";
        std::string text;
        std::filesystem::path output = "infer_outputs/output.wav";
        float speed = 1.0f;
        std::string device = "cpu";
        bool clean_text = false;
    };

    struct SessionCommand
    {
        bool exit = false;
        bool help = false;
        bool list_speakers = false;
        bool speaker_set = false;
        std::string speaker;
        bool speed_set = false;
        float speed = 1.0f;
        bool output_set = false;
        std::filesystem::path output;
        bool clean_text_set = false;
        bool clean_text = false;
        bool text_set = false;
        std::string text;
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

    std::string to_lower_ascii(std::string value)
    {
        for (char& ch : value)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return value;
    }

#ifdef _WIN32
    std::wstring utf8_to_wide(const std::string& value)
    {
        const int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (wide_length <= 0)
        {
            throw std::runtime_error("Invalid UTF-8 string.");
        }

        std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), wide_length) <= 0)
        {
            throw std::runtime_error("Failed to convert UTF-8 string.");
        }
        return wide;
    }

    std::wstring sanitize_component(std::wstring value)
    {
        for (wchar_t& ch : value)
        {
            switch (ch)
            {
            case L'<':
            case L'>':
            case L':':
            case L'"':
            case L'/':
            case L'\\':
            case L'|':
            case L'?':
            case L'*':
                ch = L'_';
                break;
            default:
                break;
            }
        }
        return value;
    }
#endif

    bool starts_with_ascii(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    bool parse_bool_token(const std::string& value)
    {
        const std::string lower = to_lower_ascii(trim_ascii(value));
        if (lower.empty() || lower == "1" || lower == "true" || lower == "yes" || lower == "on")
        {
            return true;
        }
        if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
        {
            return false;
        }
        throw std::runtime_error("Invalid boolean value: " + value);
    }

    std::vector<std::string> tokenize_session_line(const std::string& line)
    {
        std::vector<std::string> tokens;
        std::string current;
        bool in_quotes = false;
        bool escape = false;

        for (char ch : line)
        {
            if (escape)
            {
                current.push_back(ch);
                escape = false;
                continue;
            }

            if (in_quotes && ch == '\\')
            {
                escape = true;
                continue;
            }

            if (ch == '"')
            {
                in_quotes = !in_quotes;
                continue;
            }

            if (!in_quotes && std::isspace(static_cast<unsigned char>(ch)) != 0)
            {
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }
                continue;
            }

            current.push_back(ch);
        }

        if (escape)
        {
            current.push_back('\\');
        }

        if (in_quotes)
        {
            throw std::runtime_error("Unterminated quoted string in session command.");
        }

        if (!current.empty())
        {
            tokens.push_back(current);
        }

        return tokens;
    }

    std::string join_tokens(const std::vector<std::string>& tokens, std::size_t first)
    {
        std::string result;
        for (std::size_t i = first; i < tokens.size(); ++i)
        {
            if (!result.empty())
            {
                result.push_back(' ');
            }
            result.append(tokens[i]);
        }
        return result;
    }

    bool token_is_bool_value(const std::string& token)
    {
        const std::string lower = to_lower_ascii(token);
        return lower == "1" || lower == "0" || lower == "true" || lower == "false" || lower == "yes" || lower == "no" || lower == "on" || lower == "off";
    }

    SessionCommand parse_session_command(const std::string& line)
    {
        SessionCommand cmd;
        const std::vector<std::string> tokens = tokenize_session_line(line);
        if (tokens.empty())
        {
            cmd.exit = true;
            return cmd;
        }

        const std::string first = tokens.front();
        const std::string first_lower = to_lower_ascii(first);
        if (tokens.size() == 1 && (first_lower == "exit" || first_lower == "quit" || first_lower == ":q" || first_lower == ":quit"))
        {
            cmd.exit = true;
            return cmd;
        }

        if (tokens.size() == 1 && (first_lower == "--help" || first_lower == "-h" || first_lower == "/?" || first_lower == "help"))
        {
            cmd.help = true;
            return cmd;
        }

        if (tokens.size() == 1 && (first_lower == "--list-speakers" || first_lower == "list-speakers" || first_lower == "speakers"))
        {
            cmd.list_speakers = true;
            return cmd;
        }

        const bool command_like_start =
            starts_with_ascii(first_lower, "--") ||
            first_lower == "speaker" ||
            first_lower == "speed" ||
            first_lower == "output" ||
            first_lower == "clean-text" ||
            first_lower == "clean_text" ||
            first_lower == "text" ||
            first_lower == "say" ||
            starts_with_ascii(first_lower, "speaker=") ||
            starts_with_ascii(first_lower, "speed=") ||
            starts_with_ascii(first_lower, "output=") ||
            starts_with_ascii(first_lower, "clean-text=") ||
            starts_with_ascii(first_lower, "clean_text=") ||
            starts_with_ascii(first_lower, "text=") ||
            starts_with_ascii(first_lower, "say=");

        if (!command_like_start)
        {
            cmd.text_set = true;
            cmd.text = join_tokens(tokens, 0);
            return cmd;
        }

        std::size_t i = 0;
        while (i < tokens.size())
        {
            const std::string token = tokens[i];
            const std::string lower = to_lower_ascii(token);

            if (lower == "--help" || lower == "-h" || lower == "/?" || lower == "help")
            {
                cmd.help = true;
                ++i;
                continue;
            }

            if (lower == "--list-speakers" || lower == "list-speakers" || lower == "speakers")
            {
                cmd.list_speakers = true;
                ++i;
                continue;
            }

            if (lower == "--session" || lower == "--send" || lower == "--terminate")
            {
                ++i;
                continue;
            }

            if (lower == "--model" || starts_with_ascii(lower, "--model=") || starts_with_ascii(lower, "model="))
            {
                if (lower == "--model" && i + 1 < tokens.size())
                {
                    ++i;
                }
                ++i;
                continue;
            }

            if (lower == "--device" || starts_with_ascii(lower, "--device=") || starts_with_ascii(lower, "device="))
            {
                if (lower == "--device" && i + 1 < tokens.size())
                {
                    ++i;
                }
                ++i;
                continue;
            }

            auto parse_setting_value = [&](const std::string& bare_name) -> std::string
            {
                const std::string dashed_name = "--" + bare_name;
                if (lower == bare_name || lower == dashed_name)
                {
                    if (i + 1 >= tokens.size())
                    {
                        throw std::runtime_error("Missing value for " + bare_name);
                    }
                    ++i;
                    return tokens[i];
                }

                if (starts_with_ascii(lower, bare_name + "="))
                {
                    return token.substr(bare_name.size() + 1);
                }

                if (starts_with_ascii(lower, dashed_name + "="))
                {
                    return token.substr(dashed_name.size() + 1);
                }

                throw std::runtime_error("Malformed session token: " + token);
            };

            auto parse_optional_bool = [&](const std::string& bare_name) -> bool
            {
                const std::string dashed_name = "--" + bare_name;
                if (starts_with_ascii(lower, bare_name + "="))
                {
                    return parse_bool_token(token.substr(bare_name.size() + 1));
                }
                if (starts_with_ascii(lower, dashed_name + "="))
                {
                    return parse_bool_token(token.substr(dashed_name.size() + 1));
                }
                if (lower == bare_name || lower == dashed_name)
                {
                    if (i + 1 < tokens.size() && token_is_bool_value(tokens[i + 1]))
                    {
                        ++i;
                        return parse_bool_token(tokens[i]);
                    }
                    return true;
                }
                throw std::runtime_error("Malformed session token: " + token);
            };

            if (lower == "--speaker" || lower == "speaker" || starts_with_ascii(lower, "--speaker=") || starts_with_ascii(lower, "speaker="))
            {
                cmd.speaker_set = true;
                cmd.speaker = parse_setting_value("speaker");
                ++i;
                continue;
            }

            if (lower == "--speed" || lower == "speed" || starts_with_ascii(lower, "--speed=") || starts_with_ascii(lower, "speed="))
            {
                cmd.speed_set = true;
                cmd.speed = std::stof(parse_setting_value("speed"));
                ++i;
                continue;
            }

            if (lower == "--output" || lower == "output" || starts_with_ascii(lower, "--output=") || starts_with_ascii(lower, "output="))
            {
                cmd.output_set = true;
                cmd.output = parse_setting_value("output");
                ++i;
                continue;
            }

            if (lower == "--clean-text" || lower == "clean-text" || lower == "clean_text" ||
                starts_with_ascii(lower, "--clean-text=") || starts_with_ascii(lower, "clean-text=") || starts_with_ascii(lower, "clean_text="))
            {
                cmd.clean_text_set = true;
                cmd.clean_text = parse_optional_bool("clean-text");
                ++i;
                continue;
            }

            if (lower == "--text" || lower == "text" || lower == "say" ||
                starts_with_ascii(lower, "--text=") || starts_with_ascii(lower, "text=") || starts_with_ascii(lower, "say="))
            {
                cmd.text_set = true;
                if (starts_with_ascii(lower, "--text="))
                {
                    cmd.text = token.substr(7);
                }
                else if (starts_with_ascii(lower, "text="))
                {
                    cmd.text = token.substr(5);
                }
                else if (starts_with_ascii(lower, "say="))
                {
                    cmd.text = token.substr(4);
                }
                else
                {
                    cmd.text.clear();
                }

                if (i + 1 < tokens.size())
                {
                    const std::string rest = join_tokens(tokens, i + 1);
                    if (!rest.empty())
                    {
                        if (!cmd.text.empty())
                        {
                            cmd.text.push_back(' ');
                        }
                        cmd.text.append(rest);
                    }
                }
                return cmd;
            }

            if (cmd.text_set)
            {
                if (!cmd.text.empty())
                {
                    cmd.text.push_back(' ');
                }
                cmd.text.append(token);
            }
            else
            {
                cmd.text_set = true;
                cmd.text = token;
            }
            ++i;
        }

        return cmd;
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

    std::filesystem::path executable_path()
    {
#ifdef _WIN32
        wchar_t buffer[32768];
        const DWORD len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
        if (len == 0 || len >= sizeof(buffer) / sizeof(buffer[0]))
        {
            throw std::runtime_error("Failed to resolve executable path.");
        }
        return std::filesystem::path(buffer);
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

    std::filesystem::path resolve_output_path(const std::filesystem::path& output_root, const std::filesystem::path& output)
    {
        if (output.is_relative())
        {
            return output_root / output;
        }
        return output;
    }

    bool stdin_is_interactive()
    {
#ifdef _WIN32
        return _isatty(_fileno(stdin)) != 0;
#else
        return false;
#endif
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
            else if (arg == "--session")
            {
                opts.session = true;
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    opts.session_name = next_value("--session");
                }
            }
            else if (arg.rfind("--session=", 0) == 0)
            {
                opts.session = true;
                opts.session_name = arg.substr(10);
            }
            else if (arg == "--send")
            {
                opts.send = true;
            }
            else if (arg == "--terminate")
            {
                opts.terminate = true;
            }
            else if (arg == "--session-name")
            {
                opts.session_name = next_value("--session-name");
            }
            else if (arg.rfind("--session-name=", 0) == 0)
            {
                opts.session_name = arg.substr(15);
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
            else if (arg == "--named-session-server")
            {
                opts.session_server = true;
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

    std::string synthesize_once(kit::KittenTtsEngine& engine, const CliOptions& opts, std::ostream& out = std::cout)
    {
        std::ostringstream response;
        const auto result = engine.synthesize(opts.text, opts.speaker, opts.speed, opts.clean_text);
        if (!opts.output.parent_path().empty())
        {
            std::filesystem::create_directories(opts.output.parent_path());
        }
        kit::write_wav_pcm16(opts.output.string(), result.samples, result.sample_rate);
        response << "Saved " << opts.output.string() << "\n";
        response << "Model " << engine.model_name() << ", voice " << result.voice_display_name << "\n";
        out << response.str();
        kit::play_wav_file(opts.output);
        return response.str();
    }

    void run_text_loop(kit::KittenTtsEngine& engine, const CliOptions& opts, const std::filesystem::path& output_root)
    {
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
            run_opts.output = resolve_output_path(output_root, run_opts.output);
            synthesize_once(engine, run_opts);
        }
    }

    void run_session_loop(kit::KittenTtsEngine& engine, CliOptions session_defaults, const std::filesystem::path& output_root)
    {
        const bool interactive_input = stdin_is_interactive();
        if (interactive_input)
        {
            std::cout << "Session mode. Enter commands like `speaker=Jasper text=\"Hello world\"`.\n";
            std::cout << "Use `speaker=...`, `speed=...`, `output=...`, `clean-text=...`, `--list-speakers`, or `exit`.\n";
        }

        std::string line;
        while (true)
        {
            if (interactive_input)
            {
                std::cout << "session> " << std::flush;
            }

            if (!std::getline(std::cin, line))
            {
                break;
            }

            const std::string trimmed = trim_ascii(line);
            if (trimmed.empty())
            {
                if (interactive_input)
                {
                    break;
                }
                continue;
            }

            const SessionCommand request = parse_session_command(trimmed);
            if (request.exit)
            {
                break;
            }

            if (request.help)
            {
                print_cli_help(std::cout);
                continue;
            }

            if (request.list_speakers)
            {
                engine.print_speakers();
                if (!request.text_set && !request.speaker_set && !request.speed_set && !request.output_set && !request.clean_text_set)
                {
                    continue;
                }
            }

            if (request.speaker_set)
            {
                session_defaults.speaker = request.speaker;
            }
            if (request.speed_set)
            {
                session_defaults.speed = request.speed;
            }
            if (request.output_set)
            {
                session_defaults.output = request.output;
            }
            if (request.clean_text_set)
            {
                session_defaults.clean_text = request.clean_text;
            }

            if (request.text_set)
            {
                CliOptions run_opts = session_defaults;
                run_opts.text = request.text;
                run_opts.output = resolve_output_path(output_root, run_opts.output);
                synthesize_once(engine, run_opts);
            }
            else if (request.speaker_set || request.speed_set || request.output_set || request.clean_text_set)
            {
                std::cout << "Session defaults updated.\n";
            }
        }
    }

    struct SessionExecResult
    {
        bool exit = false;
        std::string response;
    };

    std::uint64_t fnv1a64(const std::string& value)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (unsigned char ch : value)
        {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    std::string quote_session_token(const std::string& token)
    {
        if (token.empty())
        {
            return "\"\"";
        }

        const bool safe = token.find_first_of(" \t\r\n\v\"\\") == std::string::npos;
        if (safe)
        {
            return token;
        }

        std::string quoted;
        quoted.push_back('"');
        for (char ch : token)
        {
            if (ch == '\\' || ch == '"')
            {
                quoted.push_back('\\');
            }
            quoted.push_back(ch);
        }
        quoted.push_back('"');
        return quoted;
    }

    bool session_trace_enabled()
    {
        return std::getenv("KITTEN_TTS_SESSION_DEBUG") != nullptr;
    }

    void append_session_trace(const std::string& message)
    {
        if (!session_trace_enabled())
        {
            return;
        }

        const std::filesystem::path log_path = std::filesystem::temp_directory_path() / "portable_kittentts_cpp_session.log";
        std::ofstream log(log_path, std::ios::app | std::ios::binary);
        if (!log)
        {
            return;
        }

        log << "[" << static_cast<unsigned long>(GetCurrentProcessId()) << "] " << message << "\n";
    }

    std::wstring make_named_session_key(const std::string& session_name)
    {
        const std::string trimmed = to_lower_ascii(trim_ascii(session_name));
        if (trimmed.empty())
        {
            throw std::runtime_error("Session name cannot be empty.");
        }

        const std::wstring clean_name = sanitize_component(utf8_to_wide(trimmed));
        const std::uint64_t hash = fnv1a64(trimmed);

        std::wostringstream out;
        out << L"portable_kittentts_cpp_session_" << clean_name << L"_"
            << std::hex << std::setw(16) << std::setfill(L'0') << hash;
        return out.str();
    }

    std::wstring make_named_session_pipe_name(const std::string& session_name)
    {
        return L"\\\\.\\pipe\\" + make_named_session_key(session_name);
    }

    std::wstring make_named_session_mutex_name(const std::string& session_name)
    {
        return L"Local\\" + make_named_session_key(session_name);
    }

    bool read_exact_handle(HANDLE handle, void* data, std::size_t size)
    {
        auto* bytes = static_cast<std::uint8_t*>(data);
        std::size_t remaining = size;
        while (remaining != 0)
        {
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
            DWORD read = 0;
            if (!ReadFile(handle, bytes, chunk, &read, nullptr))
            {
                return false;
            }
            if (read == 0)
            {
                return false;
            }
            bytes += read;
            remaining -= read;
        }
        return true;
    }

    bool write_exact_handle(HANDLE handle, const void* data, std::size_t size)
    {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        std::size_t remaining = size;
        while (remaining != 0)
        {
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
            DWORD written = 0;
            if (!WriteFile(handle, bytes, chunk, &written, nullptr))
            {
                return false;
            }
            if (written == 0)
            {
                return false;
            }
            bytes += written;
            remaining -= written;
        }
        return true;
    }

    std::string read_length_prefixed_message(HANDLE handle)
    {
        std::uint32_t length = 0;
        if (!read_exact_handle(handle, &length, sizeof(length)))
        {
            throw std::runtime_error("Failed to read the session message length.");
        }

        if (length > 8U * 1024U * 1024U)
        {
            throw std::runtime_error("Session message is too large.");
        }

        std::string message(length, '\0');
        if (length != 0 && !read_exact_handle(handle, message.data(), message.size()))
        {
            throw std::runtime_error("Failed to read the session message.");
        }
        return message;
    }

    void write_length_prefixed_message(HANDLE handle, const std::string& message)
    {
        const std::uint32_t length = static_cast<std::uint32_t>(message.size());
        if (!write_exact_handle(handle, &length, sizeof(length)))
        {
            throw std::runtime_error("Failed to write the session message length.");
        }
        if (!message.empty() && !write_exact_handle(handle, message.data(), message.size()))
        {
            throw std::runtime_error("Failed to write the session message.");
        }
    }

    std::string build_ipc_request_from_argv(int argc, char** argv)
    {
        std::string request;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--send" || arg == "--terminate" || arg == "--session" || arg == "--session-name" || arg == "--named-session-server")
            {
                if ((arg == "--session" || arg == "--session-name") && i + 1 < argc && argv[i + 1][0] != '-')
                {
                    ++i;
                }
                continue;
            }

            if (arg.rfind("--session=", 0) == 0)
            {
                continue;
            }

            if (arg.rfind("--session-name=", 0) == 0)
            {
                continue;
            }

            if (!request.empty())
            {
                request.push_back(' ');
            }
            request += quote_session_token(arg);
        }
        return request;
    }

    SessionExecResult execute_session_command(
        kit::KittenTtsEngine& engine,
        CliOptions& session_defaults,
        const SessionCommand& request,
        const std::filesystem::path& output_root)
    {
        SessionExecResult result;
        std::ostringstream response;

        if (request.help)
        {
            print_cli_help(response);
            result.response = response.str();
            return result;
        }

        if (request.list_speakers)
        {
            response << engine.speakers_text();
            if (!request.text_set && !request.speaker_set && !request.speed_set && !request.output_set && !request.clean_text_set)
            {
                result.response = response.str();
                return result;
            }
        }

        if (request.exit)
        {
            result.exit = true;
            response << "Session closed.\n";
            result.response = response.str();
            return result;
        }

        if (request.speaker_set)
        {
            session_defaults.speaker = request.speaker;
        }
        if (request.speed_set)
        {
            session_defaults.speed = request.speed;
        }
        if (request.output_set)
        {
            session_defaults.output = request.output;
        }
        if (request.clean_text_set)
        {
            session_defaults.clean_text = request.clean_text;
        }

        if (request.text_set)
        {
            CliOptions run_opts = session_defaults;
            run_opts.text = request.text;
            run_opts.output = resolve_output_path(output_root, run_opts.output);
            synthesize_once(engine, run_opts, response);
        }
        else if (request.speaker_set || request.speed_set || request.output_set || request.clean_text_set)
        {
            response << "Session defaults updated.\n";
        }

        result.response = response.str();
        return result;
    }

    void run_named_session_server(
        kit::KittenTtsEngine& engine,
        CliOptions session_defaults,
        const std::filesystem::path& output_root,
        const std::string& session_name)
    {
        const std::wstring pipe_name = make_named_session_pipe_name(session_name);
        const std::wstring mutex_name = make_named_session_mutex_name(session_name);
        append_session_trace("server entering loop for session '" + session_name + "'");

        HANDLE mutex = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
        if (!mutex)
        {
            throw std::runtime_error("Failed to create the named session mutex.");
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(mutex);
            throw std::runtime_error("A named session is already running: " + session_name);
        }

        std::cout << "Named session '" << session_name << "' ready.\n";
        std::cout << "Send commands with: kitten_tts.exe --session " << session_name << " --text \"Hello world\"\n";
        std::cout << "Stop it with: kitten_tts.exe --session " << session_name << " --terminate\n";

        bool keep_running = true;
        while (keep_running)
        {
            HANDLE pipe = CreateNamedPipeW(
                pipe_name.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,
                65536,
                65536,
                0,
                nullptr);
            if (pipe == INVALID_HANDLE_VALUE)
            {
                CloseHandle(mutex);
                throw std::runtime_error("Failed to create the named session pipe.");
            }

            BOOL connected = ConnectNamedPipe(pipe, nullptr);
            if (!connected)
            {
                const DWORD error = GetLastError();
                if (error != ERROR_PIPE_CONNECTED)
                {
                    CloseHandle(pipe);
                    continue;
                }
            }

            std::string response;
            try
            {
                const std::string request_line = read_length_prefixed_message(pipe);
                append_session_trace("server received request for session '" + session_name + "': " + request_line);
                const SessionCommand request = parse_session_command(request_line);
                const SessionExecResult exec = execute_session_command(engine, session_defaults, request, output_root);
                response = exec.response;
                keep_running = !exec.exit;
            }
            catch (const std::exception& e)
            {
                response = std::string("ERROR: ") + e.what() + "\n";
            }

            try
            {
                write_length_prefixed_message(pipe, response);
                FlushFileBuffers(pipe);
            }
            catch (...)
            {
            }

            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }

        CloseHandle(mutex);
    }

    std::string send_named_session_request(const std::string& session_name, const std::string& request_line)
    {
        const std::wstring pipe_name = make_named_session_pipe_name(session_name);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);

        while (true)
        {
            HANDLE pipe = CreateFileW(
                pipe_name.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);

            if (pipe != INVALID_HANDLE_VALUE)
            {
                try
                {
                    write_length_prefixed_message(pipe, request_line);
                    FlushFileBuffers(pipe);
                    const std::string response = read_length_prefixed_message(pipe);
                    CloseHandle(pipe);
                    return response;
                }
                catch (...)
                {
                    CloseHandle(pipe);
                    throw;
                }
            }

            const DWORD error = GetLastError();
            if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND)
            {
                throw std::runtime_error("Failed to connect to named session '" + session_name + "'.");
            }

            if (std::chrono::steady_clock::now() >= deadline)
            {
                throw std::runtime_error("Timed out waiting for named session '" + session_name + "'.");
            }

            if (!WaitNamedPipeW(pipe_name.c_str(), 500))
            {
                Sleep(50);
            }
        }
    }

    bool named_session_is_running(const std::string& session_name)
    {
        const std::wstring mutex_name = make_named_session_mutex_name(session_name);
        HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, mutex_name.c_str());
        if (!mutex)
        {
            return false;
        }
        CloseHandle(mutex);
        return true;
    }

    void launch_named_session_server_process(const CliOptions& opts, const std::filesystem::path& output_root)
    {
        const std::filesystem::path self = executable_path();

        const std::wstring output_root_value = output_root.wstring();
        if (!SetEnvironmentVariableW(L"KITTEN_TTS_OUTPUT_ROOT", output_root_value.c_str()))
        {
            throw std::runtime_error("Failed to set KITTEN_TTS_OUTPUT_ROOT for the session server.");
        }

        std::wostringstream command_line;
        command_line << L"\"" << self.wstring() << L"\" --named-session-server --session-name " << utf8_to_wide(opts.session_name)
            << L" --model " << utf8_to_wide(opts.model);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION process{};
        std::wstring command_line_buffer = command_line.str();
        const DWORD creation_flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP;
        append_session_trace("launching hidden server for session '" + opts.session_name + "' model '" + opts.model + "'");
        const BOOL created = CreateProcessW(
            self.c_str(),
            command_line_buffer.empty() ? nullptr : command_line_buffer.data(),
            nullptr,
            nullptr,
            FALSE,
            creation_flags,
            nullptr,
            nullptr,
            &startup,
            &process);

        const DWORD launch_error = GetLastError();

        if (!created)
        {
            append_session_trace("CreateProcessW failed for session '" + opts.session_name + "' error " + std::to_string(static_cast<unsigned long>(launch_error)));
            throw std::runtime_error("Failed to launch the named session server. Error " + std::to_string(static_cast<unsigned long>(launch_error)) + ".");
        }

        append_session_trace("spawned server pid " + std::to_string(static_cast<unsigned long>(process.dwProcessId)) + " for session '" + opts.session_name + "'");

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }

    void ensure_named_session_server(const CliOptions& opts, const std::filesystem::path& output_root)
    {
        if (named_session_is_running(opts.session_name))
        {
            return;
        }

        launch_named_session_server_process(opts, output_root);
    }
}

int main(int argc, char** argv)
{
    try
    {
        CliOptions opts = parse_args(argc, argv);
        if (opts.help)
        {
            print_cli_help(std::cout);
            return 0;
        }

        if (opts.session_server)
        {
            if (opts.session_name.empty())
            {
                throw std::runtime_error("Missing value for --session NAME.");
            }

            append_session_trace("session_server mode entered for session '" + opts.session_name + "'");

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
            run_named_session_server(engine, opts, output_root, opts.session_name);
            return 0;
        }

        const bool named_session_requested = !opts.session_name.empty();
        const bool named_session_client =
            named_session_requested &&
            (
                opts.send ||
                opts.terminate ||
                !opts.text.empty() ||
                opts.list_speakers
            );

        if ((opts.send || opts.terminate) && opts.session_name.empty())
        {
            throw std::runtime_error("Missing value for --session NAME.");
        }

        if (named_session_client)
        {
            const auto app_root = resolve_app_root();
            const auto output_root = resolve_output_root(app_root);

            if (opts.terminate)
            {
                if (!named_session_is_running(opts.session_name))
                {
                    throw std::runtime_error("No named session is running: " + opts.session_name);
                }

                const std::string response = send_named_session_request(opts.session_name, "exit");
                std::cout << response;
                if (response.rfind("ERROR:", 0) == 0)
                {
                    return 1;
                }
                return 0;
            }

            if (!named_session_is_running(opts.session_name))
            {
                launch_named_session_server_process(opts, output_root);
            }

            const std::string request_line = build_ipc_request_from_argv(argc, argv);
            if (request_line.empty())
            {
                throw std::runtime_error("No session command was provided.");
            }

            const std::string response = send_named_session_request(opts.session_name, request_line);
            std::cout << response;
            if (response.rfind("ERROR:", 0) == 0)
            {
                return 1;
            }
            return 0;
        }

        if (named_session_requested && !opts.session)
        {
            throw std::runtime_error("--session NAME must be used with --session, --send, or --terminate.");
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

        if (opts.list_speakers && !opts.session)
        {
            engine.print_speakers();
            return 0;
        }

        if (opts.session && opts.list_speakers)
        {
            engine.print_speakers();
        }

        if (!opts.text.empty())
        {
            synthesize_once(engine, opts);
            if (!opts.session)
            {
                return 0;
            }
        }

        if (opts.session)
        {
            if (!opts.session_name.empty())
            {
                run_named_session_server(engine, opts, output_root, opts.session_name);
            }
            else
            {
                run_session_loop(engine, opts, output_root);
            }
            return 0;
        }

        run_text_loop(engine, opts, output_root);

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
